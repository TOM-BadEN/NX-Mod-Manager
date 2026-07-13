/**
 * FtpManager - FTP 文件传输模块
 * 薄封装层，管理 libftpsrv 的启停
 */

#include "core/ftp.hpp"
#include "utils/fsHelper.hpp"
#include "utils/http.hpp"
#include "ftpsrv_thread.h"
#include "ftpsrv.h"
#include "vfs_sd.h"
#include <switch.h>
#include <arpa/inet.h>
#include <cstring>
#include <borealis/core/i18n.hpp>

// C 函数指针桥接：libftpsrv 的 log_callback 是纯 C 函数指针，无法捕获状态，
// 通过 static 指针转发到当前活跃的 FtpManager 实例
static FtpManager* s_activeInstance = nullptr;

static void ftpLogCallback(enum FTP_API_LOG_TYPE type, const char* msg) {
    if (s_activeInstance) s_activeInstance->handleLog(static_cast<int>(type), msg);
}

/**
 * 动态切换 socket 缓冲池配置
 *
 * borealis 框架在 applet 模式下默认 sb_efficiency=1（池子仅 ~563KB），
 * 不足以支撑多线程 FTP 传输（4线程需要 ~7MB+）。
 * 因此在 FTP 启动时重新初始化为大池子，退出时恢复框架默认，避免常驻内存浪费。
 *
 * socketExit() 会销毁当前所有 socket（包括 nxlink）。
 */
static void socketReconfigure(bool ftpMode) {
    http::suspend();
    socketExit();
    if (ftpMode) {
        // 池子 ≈ (4MB + 4MB + UDP) × 12 ≈ 96MB（仅 FTP 运行期间占用）
        const SocketInitConfig config = {
            .tcp_tx_buf_size     = 1024 * 64,       // 64KB 初始发送缓冲
            .tcp_rx_buf_size     = 1024 * 64,       // 64KB 初始接收缓冲
            .tcp_tx_buf_max_size = 1024 * 1024 * 4, // 4MB 发送上限（TCP 自动调优）
            .tcp_rx_buf_max_size = 1024 * 1024 * 4, // 4MB 接收上限（决定上传速度）
            .udp_tx_buf_size     = 0x2400,          // UDP 发送缓冲（libnx 默认值）
            .udp_rx_buf_size     = 0xA500,          // UDP 接收缓冲（libnx 默认值）
            .sb_efficiency       = 12,              // 缓冲池倍率（池子可容纳 ~12 个全开 socket）
            .num_bsd_sessions    = 3,               // BSD 并发会话数（FTP poll 模型只需少量）
            .bsd_service_type    = BsdServiceType_Auto,
        };
        socketInitialize(&config);
    } else {
        // 恢复 http::init() 中的配置
        SocketInitConfig cfg = *(socketGetDefaultInitConfig());
        cfg.tcp_rx_buf_max_size = 1024 * 512;  // 512KB, 2x libnx 默认值
        cfg.num_bsd_sessions   = 3;
        cfg.sb_efficiency      = 4;
        socketInitialize(&cfg);
    }
    http::resume();
}

FtpManager::~FtpManager() {
    if (m_running) {
        stop();
    }
}

FtpStartResult FtpManager::start(FtpConfig config, EventCallback onEvent) {
    FtpStartResult result;

    if (m_running) {
        result.errorMessage = brls::getStr("other/ftp/alreadyRunning");
        return result;
    }

    u32 ipAddress;
    if (R_FAILED(nifmGetCurrentIpAddress(&ipAddress))) {
        result.errorMessage = brls::getStr("other/ftp/noIpAddress");
        return result;
    }

    socketReconfigure(true);

    /* 快捷目录配置：将 C++ FtpMount 转为 C FtpVfsMountEntry 传给 VFS 层 */
    if (!config.mounts.empty()) {
        FtpVfsMountEntry entries[FTP_VFS_MAX_MOUNTS] = {};
        int count = static_cast<int>(config.mounts.size());
        if (count > FTP_VFS_MAX_MOUNTS) count = FTP_VFS_MAX_MOUNTS;
        for (int i = 0; i < count; i++) {
            fs::ensureDir(config.mounts[i].basePath);
            std::strncpy(entries[i].displayName, config.mounts[i].displayName.c_str(), FTP_VFS_NAME_MAX - 1);
            std::strncpy(entries[i].basePath, config.mounts[i].basePath.c_str(), FS_MAX_PATH - 1);
        }
        ftp_vfs_set_mounts(entries, count);
    }

    m_onEvent = std::move(onEvent);
    m_successCount = 0;
    m_failedCount = 0;
    m_transferring = false;
    s_activeInstance = this;

    struct FtpSrvConfig ftpConfig = {};
    std::strncpy(ftpConfig.user, config.user.c_str(), sizeof(ftpConfig.user) - 1);
    std::strncpy(ftpConfig.pass, config.password.c_str(), sizeof(ftpConfig.pass) - 1);
    ftpConfig.port = config.port;
    ftpConfig.anon = config.anonymous;
    ftpConfig.log_callback = ftpLogCallback;

    if (ftpsrv_start(&ftpConfig) < 0) {
        s_activeInstance = nullptr;
        m_onEvent = nullptr;
        ftp_vfs_clear_mounts();
        socketReconfigure(false);
        result.errorMessage = brls::getStr("other/ftp/startFailed");
        return result;
    }

    m_running = true;
    result.success = true;

    struct in_addr addr = {ipAddress};
    result.ipAddress = inet_ntoa(addr);

    return result;
}

void FtpManager::stop() {
    if (!m_running) return;
    s_activeInstance = nullptr;
    ftpsrv_stop();
    ftp_vfs_clear_mounts();
    socketReconfigure(false);
    m_running = false;
    m_onEvent = nullptr;
}

bool FtpManager::isRunning() const {
    return m_running;
}

// FTP 子线程调用，解析 libftpsrv 的 log_callback 并转换为 FtpEvent 派发给调用方
void FtpManager::handleLog(int type, const char* msg) {
    if (!m_onEvent) return;

    FtpEventData data;
    bool dispatch = false;

    if (type == FTP_API_LOG_TYPE_RESPONSE) {
        // 220: 客户端 TCP 连接成功，服务端发送欢迎消息
        if (std::strncmp(msg, "220 ", 4) == 0) {
            data.event = FtpEvent::Connected;
            dispatch = true;
        // 221: 客户端发送 QUIT，服务端确认关闭连接
        } else if (std::strncmp(msg, "221 ", 4) == 0) {
            data.event = FtpEvent::Disconnected;
            dispatch = true;
        // 226: 数据传输完成（仅在 STOR/RETR 后匹配，避免误计 LIST 的 226）
        } else if (std::strncmp(msg, "226 ", 4) == 0 && m_transferring) {
            m_transferring = false;
            m_successCount++;
            data.event = FtpEvent::TransferEnd;
            data.successCount = m_successCount;
            data.failedCount = m_failedCount;
            dispatch = true;
        }
    } else if (type == FTP_API_LOG_TYPE_ERROR) {
        // 426: 数据传输中断（仅在 STOR/RETR 后匹配）
        if (std::strncmp(msg, "426 ", 4) == 0 && m_transferring) {
            m_transferring = false;
            m_failedCount++;
            data.event = FtpEvent::TransferFailed;
            data.successCount = m_successCount;
            data.failedCount = m_failedCount;
            dispatch = true;
        }
    } else if (type == FTP_API_LOG_TYPE_COMMAND) {
        // STOR: 客户端上传文件（PC → Switch）  RETR: 客户端下载文件（Switch → PC）
        if (std::strcmp(msg, "STOR") == 0 || std::strcmp(msg, "RETR") == 0) {
            m_transferring = true;
            data.event = FtpEvent::TransferBegin;
            data.successCount = m_successCount;
            data.failedCount = m_failedCount;
            dispatch = true;
        }
    }

    if (dispatch) m_onEvent(data);
}
