#include "utils/http.hpp"
#include <curl/curl.h>
#include <switch.h>
#include <mutex>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <utility>

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

/** @brief 创建 libcurl share handle，用于安全共享 DNS / SSL session */
static void createShare() {
    if (g_share) return;

    g_share = curl_share_init();
    if (!g_share) return;

    curl_share_setopt(g_share, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
    curl_share_setopt(g_share, CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);
    curl_share_setopt(g_share, CURLSHOPT_LOCKFUNC, shareLock);
    curl_share_setopt(g_share, CURLSHOPT_UNLOCKFUNC, shareUnlock);
}

/** @brief 清理 libcurl share handle */
static void cleanupShare() {
    if (!g_share) return;
    curl_share_cleanup(g_share);
    g_share = nullptr;
}

/** @brief 记录当前线程的 curl slot，便于 suspend()/cleanup() 统一清理 */
static void registerCurlSlotLocked(ThreadCurl* slot) {
    if (slot->registered) return;
    g_curlSlots.push_back(slot);
    slot->registered = true;
}

/** @brief 移除当前线程的 curl slot 注册记录 */
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

/** @brief 清理某个线程持有的 curl easy handle */
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

/** @brief 获取当前线程复用的 curl easy handle */
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
        for (auto* slot : g_curlSlots) {
            cleanupCurlSlot(slot);
        }
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

/** @brief libcurl 数据回调：把响应体追加到内存 buffer */
static size_t writeMemoryCallback(void* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* buf = static_cast<std::vector<uint8_t>*>(userdata);
    auto bytes = size * nmemb;
    buf->insert(buf->end(), (uint8_t*)ptr, (uint8_t*)ptr + bytes);
    return bytes;
}

struct StreamData {
    const std::function<bool(const uint8_t* data, size_t size)>* callback;
};

/** @brief libcurl 数据回调：把响应体按块交给调用方 */
static size_t writeStreamCallback(void* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* data = static_cast<StreamData*>(userdata);
    auto bytes = size * nmemb;
    if (!data->callback || !*data->callback) return bytes;
    return (*data->callback)(static_cast<uint8_t*>(ptr), bytes) ? bytes : 0;
}

/** @brief 去掉响应头 value 两端空白和换行 */
static std::string trimHeaderValue(std::string value) {
    while (!value.empty() && (value.back() == '\r' || value.back() == '\n' || std::isspace(static_cast<unsigned char>(value.back())))) {
        value.pop_back();
    }

    size_t begin = 0;
    while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin]))) {
        begin++;
    }

    if (begin > 0) value.erase(0, begin);
    return value;
}

/** @brief 将响应头 name 转成小写，便于上层按固定 key 查找 */
static std::string lowerHeaderName(std::string name) {
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return name;
}

/** @brief libcurl 响应头回调：保存最终响应的 headers */
static size_t headerCallback(void* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* headers = static_cast<std::vector<Header>*>(userdata);
    auto bytes = size * nmemb;
    std::string line(static_cast<char*>(ptr), bytes);

    if (line.rfind("HTTP/", 0) == 0) {
        headers->clear(); // 重定向时只保留最后一次响应头
        return bytes;
    }

    auto colon = line.find(':');
    if (colon == std::string::npos) return bytes;

    Header header;
    header.name = lowerHeaderName(line.substr(0, colon));
    header.value = trimHeaderValue(line.substr(colon + 1));
    if (!header.name.empty()) headers->push_back(std::move(header));

    return bytes;
}

struct ProgressData {
    const std::function<bool(size_t total, size_t now)>* callback;
    std::stop_token* token;
};

/** @brief libcurl 进度回调：报告进度并处理取消 */
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

/** @brief libcurl 取消回调：检测到 stop_token 取消即中断传输 */
static int cancelCallback(void* userdata, curl_off_t /*dltotal*/, curl_off_t /*dlnow*/,
                          curl_off_t /*ultotal*/, curl_off_t /*ulnow*/) {
    auto* token = static_cast<std::stop_token*>(userdata);
    return (token && token->stop_requested()) ? 1 : 0;
}

// ── 内部实现 ──────────────────────────────────────────

/** @brief 设置所有请求共享的 curl 传输参数 */
static void setCommonOptions(CURL* curl, std::stop_token* token) {
    // 当前只共享 DNS/SSL session，不跨线程共享连接池。
    if (g_share) curl_easy_setopt(curl, CURLOPT_SHARE, g_share);   // 共享 DNS/SSL session
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

/** @brief 将请求头转换为 libcurl 使用的 curl_slist */
static curl_slist* createHeaders(const std::vector<Header>& requestHeaders) {
    curl_slist* headers = nullptr;

    for (const auto& header : requestHeaders) {
        if (header.name.empty()) continue;
        std::string line = header.name + ": " + header.value;
        auto* next = curl_slist_append(headers, line.c_str());
        if (next) headers = next;
    }
    return headers;
}

/** @brief 执行一次 HTTP 请求，由调用方决定响应体写入内存还是流式回调 */
static void performRequest(const Request& request, Response& response, void* writeData, size_t (*writeCallback)(void*, size_t, size_t, void*)) {
    CURL* curl = getThreadHandle();
    if (!curl) {
        response.networkCode = CURLE_FAILED_INIT;
        response.error = "curl handle unavailable";
        return;
    }

    curl_easy_reset(curl);

    curl_slist* headers = createHeaders(request.headers);
    std::stop_token token = request.token;
    ProgressData progressData{&request.progress, &token};

    setCommonOptions(curl, &token);
    if (headers) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_URL, request.url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, writeData);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response.headers);

    if (request.progress) {
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progressCallback);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progressData);
    }

    if (request.method == Method::Post) {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(request.body.size()));
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.empty() ? "" : reinterpret_cast<const char*>(request.body.data()));
    }

    CURLcode res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.statusCode);

    response.networkCode = static_cast<int>(res);
    response.cancelled = (res == CURLE_ABORTED_BY_CALLBACK && token.stop_requested());
    if (res != CURLE_OK) {
        response.error = curl_easy_strerror(res);
    }

    if (headers) curl_slist_free_all(headers);
}

// ── 公开接口 ──────────────────────────────────────────

Response requestToMemory(const Request& request) {
    Response response;
    performRequest(request, response, &response.body, writeMemoryCallback);
    return response;
}

Response requestStream(const Request& request, const std::function<bool(const uint8_t* data, size_t size)>& onData) {
    Response response;
    StreamData data{&onData};
    performRequest(request, response, &data, writeStreamCallback);
    return response;
}

} // namespace http
