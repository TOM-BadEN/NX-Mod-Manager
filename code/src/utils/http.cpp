#include "utils/http.hpp"
#include "utils/fsHelper.hpp"
#include "common/deviceInfo.hpp"
#include <curl/curl.h>
#include <switch.h>
#include <mutex>
#include <cstdio>
#include <string>
#include <vector>

namespace http {

// ── 连接复用 ──────────────────────────────────────────

static CURLSH* g_share = nullptr;
static std::mutex g_shareMutex[CURL_LOCK_DATA_LAST];

static void shareLock(CURL* /*handle*/, curl_lock_data data, curl_lock_access /*access*/, void* /*userptr*/) {
    g_shareMutex[data].lock();
}

static void shareUnlock(CURL* /*handle*/, curl_lock_data data, void* /*userptr*/) {
    g_shareMutex[data].unlock();
}

struct ThreadCurl {
    CURL* handle = nullptr;
    bool registered = false;
    ~ThreadCurl();
};

static std::mutex g_httpMutex;
static std::vector<ThreadCurl*> g_curlSlots;
static bool g_suspended = false;
static thread_local ThreadCurl tl_curl;

static void createShare() {
    if (g_share) return;

    g_share = curl_share_init();
    if (!g_share) return;

    curl_share_setopt(g_share, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
    curl_share_setopt(g_share, CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);
    curl_share_setopt(g_share, CURLSHOPT_SHARE, CURL_LOCK_DATA_CONNECT);
    curl_share_setopt(g_share, CURLSHOPT_LOCKFUNC, shareLock);
    curl_share_setopt(g_share, CURLSHOPT_UNLOCKFUNC, shareUnlock);
}

static void cleanupShare() {
    if (!g_share) return;
    curl_share_cleanup(g_share);
    g_share = nullptr;
}

static void registerCurlSlotLocked(ThreadCurl* slot) {
    if (slot->registered) return;
    g_curlSlots.push_back(slot);
    slot->registered = true;
}

static void unregisterCurlSlotLocked(ThreadCurl* slot) {
    if (!slot->registered) return;

    for (auto it = g_curlSlots.begin(); it != g_curlSlots.end(); ++it) {
        if (*it == slot) {
            g_curlSlots.erase(it);
            break;
        }
    }
    slot->registered = false;
}

static void cleanupCurlSlot(ThreadCurl* slot) {
    if (!slot || !slot->handle) return;
    curl_easy_cleanup(slot->handle);
    slot->handle = nullptr;
}

ThreadCurl::~ThreadCurl() {
    std::lock_guard lock(g_httpMutex);
    cleanupCurlSlot(this);
    unregisterCurlSlotLocked(this);
}

static CURL* getThreadHandle() {
    {
        std::lock_guard lock(g_httpMutex);
        if (g_suspended) return nullptr;
        registerCurlSlotLocked(&tl_curl);
    }

    if (!tl_curl.handle) {
        tl_curl.handle = curl_easy_init();
    }
    return tl_curl.handle;
}

// ── 初始化与清理 ──────────────────────────────────────────

/*
 * 警告：suspend()/resume() 只是 socket 重配前后的生命周期钩子，不是通用的线程安全暂停屏障。
 *
 * 当前业务约束：
 * - FTP 启动、停止、启动失败回滚都会重新配置 socket；socketExit() 会销毁底层 socket。
 * - 因此在 socketExit() 前必须释放所有可能持有 socket 的 curl 复用资源。
 * - 调用 suspend()/resume() 期间，业务层必须保证没有正在执行、也没有即将开始的 HTTP 请求。
 *
 * 为什么这个约束重要：
 * - suspend() 会清理所有已注册线程的 easy handle，以及它们挂载的 share handle。
 * - 如果某个 worker 线程正在 curl_easy_perform()，另一个线程此时清理它的 easy handle，
 *   可能导致 use-after-cleanup、崩溃或卡死。
 * - g_suspended 只能让后续进入 getThreadHandle() 的请求快速失败；它不会等待已开始的请求结束，
 *   也无法覆盖“已经拿到 handle，但还没进入 curl_easy_perform()”这段竞态窗口。
 *
 * 未来如果不再依赖这个业务保证，需要把这里改成真正的同步机制：
 * - 给每个请求入口加 active request 计数，或使用读写锁/生命周期锁。
 * - 普通请求进入时持有读侧或计数；suspend() 先关闭新请求入口，再等待 active count 归零，
 *   最后再清理 easy/share handle。
 */

void init() {
    // Borealis applet 模式默认 session=2/efficiency=1，不够并发 HTTP 下载
    // 重置为 libnx 默认值 session=3/efficiency=4
    socketExit();
    SocketInitConfig cfg = *(socketGetDefaultInitConfig());
    cfg.tcp_rx_buf_max_size = 1024 * 512;  // 512KB, 2x libnx 默认值
    cfg.num_bsd_sessions   = 3;
    cfg.sb_efficiency      = 4;
    socketInitialize(&cfg);

    curl_global_init(CURL_GLOBAL_DEFAULT);

    std::lock_guard lock(g_httpMutex);
    g_suspended = false;
    createShare();
}

void cleanup() {
    {
        std::lock_guard lock(g_httpMutex);
        g_suspended = true;
        cleanupShare();
    }
    curl_global_cleanup();
}

void suspend() {
    std::lock_guard lock(g_httpMutex);
    g_suspended = true;

    // easy handle 必须先于 share 清理，因为 handle 上挂着 CURLOPT_SHARE。
    for (auto* slot : g_curlSlots) {
        cleanupCurlSlot(slot);
    }
    cleanupShare();
}

void resume() {
    std::lock_guard lock(g_httpMutex);
    createShare();
    g_suspended = false;
}

std::string escape(const std::string& value) {
    std::string encoded;

    char* out = curl_escape(value.c_str(), static_cast<int>(value.size()));
    if (out) {
        encoded = out;
        curl_free(out);
    }
    return encoded;
}

// ── 内部回调 ──────────────────────────────────────────

// libcurl 每收到一块数据就调这个，把数据追加到 vector 里
static size_t writeMemoryCallback(void* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* buf = static_cast<std::vector<uint8_t>*>(userdata);
    auto bytes = size * nmemb;
    buf->insert(buf->end(), (uint8_t*)ptr, (uint8_t*)ptr + bytes);
    return bytes;
}

// libcurl 每收到一块数据就调这个，直接写入文件
static size_t writeFileCallback(void* ptr, size_t size, size_t nmemb, void* userdata) {
    return fwrite(ptr, size, nmemb, static_cast<FILE*>(userdata));
}

struct ProgressData {
    ProgressCallback* callback;
    std::stop_token* token;
};

// libcurl 定期调用（约每秒一次），用于报告进度和中断控制
static int progressCallback(void* userdata, curl_off_t dltotal, curl_off_t dlnow,
                     curl_off_t /*ultotal*/, curl_off_t /*ulnow*/) {
    auto* data = static_cast<ProgressData*>(userdata);
    if (data->token && data->token->stop_requested()) return 1;  // stop_token 取消
    if (data->callback && *data->callback) {
        bool cont = (*data->callback)(static_cast<size_t>(dltotal), static_cast<size_t>(dlnow));
        return cont ? 0 : 1;  // 返回非 0 中断传输
    }
    return 0;
}

// stop_token 取消回调：libcurl 定期调用，检测到取消即中断传输
static int cancelCallback(void* userdata, curl_off_t /*dltotal*/, curl_off_t /*dlnow*/,
                          curl_off_t /*ultotal*/, curl_off_t /*ulnow*/) {
    auto* token = static_cast<std::stop_token*>(userdata);
    return (token && token->stop_requested()) ? 1 : 0;
}

// ── 内部实现 ──────────────────────────────────────────

static void setCommonOptions(CURL* curl, std::stop_token* token) {
    // 当前共享 DNS/SSL/连接缓存。若后续追求更严格的 libcurl 多线程语义，可考虑不共享 CURL_LOCK_DATA_CONNECT。
    if (g_share) curl_easy_setopt(curl, CURLOPT_SHARE, g_share);   // 共享 DNS/SSL/连接缓存
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "NX-Mod-Manager");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);            // 自动跟随重定向
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);            // 跳过 SSL 证书验证
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);            // 跳过主机名验证
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);           // 连接超时 10 秒
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1024L);        // 低速阈值 1KB/s
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 30L);           // 低速持续 30 秒则断开
    curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 512 * 1024L);       // 接收缓冲区 512KB
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");           // 自动支持 gzip 压缩
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);                // 启用进度回调（用于取消检测）
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, cancelCallback);  // 取消检测回调
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, token);               // 传入 stop_token 指针
}

static curl_slist* createCommonHeaders() {
    curl_slist* headers = nullptr;

    static const std::string deviceIdHeader =
        "X-Device-ID: " + std::to_string(DeviceInfo::getDeviceId());

    headers = curl_slist_append(headers, deviceIdHeader.c_str());
    return headers;
}

static curl_slist* createPostHeaders() {
    curl_slist* headers = createCommonHeaders();
    headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
    return headers;
}

static curl_slist* createJsonHeaders() {
    curl_slist* headers = createCommonHeaders();
    headers = curl_slist_append(headers, "Content-Type: application/json");
    return headers;
}

// GET 请求到内存：复用线程句柄 → 重置 → 配置 → 执行
static Response getToMemory(const std::string& url, std::stop_token token) {
    Response resp{0, {}};

    CURL* curl = getThreadHandle();
    if (!curl) return resp;

    curl_easy_reset(curl);

    curl_slist* headers = createCommonHeaders();

    setCommonOptions(curl, &token);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp.body);

    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &resp.code);
    }

    curl_slist_free_all(headers);
    return resp;
}

static Response postToMemory(const std::string& url, const std::string& body, std::stop_token token) {
    Response resp{0, {}};

    CURL* curl = getThreadHandle();
    if (!curl) return resp;

    curl_easy_reset(curl);

    curl_slist* headers = createPostHeaders();

    setCommonOptions(curl, &token);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp.body);

    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &resp.code);
    }

    curl_slist_free_all(headers);
    return resp;
}

// ── 公开接口 ──────────────────────────────────────────

Response get(const std::string& url, std::stop_token token) {
    return getToMemory(url, token);
}

Response post(const std::string& url, const std::string& body, std::stop_token token) {
    return postToMemory(url, body, token);
}

Response postJson(const std::string& url, const std::string& json, std::stop_token token) {
    Response resp{0, {}};
    CURL* curl = getThreadHandle();
    if (!curl) return resp;
    curl_easy_reset(curl);
    curl_slist* headers = createJsonHeaders();
    setCommonOptions(curl, &token);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(json.size()));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp.body);
    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK) curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &resp.code);
    curl_slist_free_all(headers);
    return resp;
}

Response downloadImage(const std::string& url, std::stop_token token) {
    return getToMemory(url, token);
}

Response downloadToFile(const std::string& url, const std::string& path,
                        ProgressCallback onProgress, std::stop_token token) {
    Response resp{0, {}};

    CURL* curl = getThreadHandle();
    if (!curl) return resp;

    curl_easy_reset(curl);

    // 确保父目录存在
    auto slashPos = path.rfind('/');
    if (slashPos != std::string::npos && slashPos != 0) {
        fs::ensureDir(path.substr(0, slashPos));
    }

    FILE* fp = fopen(path.c_str(), "wb");
    if (!fp) return resp;
    std::setvbuf(fp, nullptr, _IOFBF, 512 * 1024);

    curl_slist* headers = createCommonHeaders();

    setCommonOptions(curl, &token);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFileCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);

    // 挂载进度回调（覆盖 setCommonOptions 中的取消回调，但保留 stop_token 检测）
    ProgressData progressData{&onProgress, &token};
    if (onProgress) {
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progressCallback);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progressData);
    }

    CURLcode res = curl_easy_perform(curl); // 执行下载（阻塞）
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &resp.code);
    }

    fclose(fp);
    curl_slist_free_all(headers);

    // 请求失败或被中断时删除不完整的文件
    if (!resp.ok()) {
        remove(path.c_str());
    }

    return resp;
}

} // namespace http
