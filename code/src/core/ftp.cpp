/**
 * FtpManager - FTP 文件传输模块实现
 */

#include "core/ftp.hpp"
#include "utils/fsHelper.hpp"
#include "utils/http.hpp"
#include "ftpsrv.h"
#include "ftpsrv_thread.h"
#include "vfs_sd.h"
#include <arpa/inet.h>
#include <borealis/core/i18n.hpp>
#include <cstring>
#include <switch.h>

// libftpsrv 的日志回调是纯 C 函数指针，通过静态指针转发给当前实例。
static FtpManager* s_activeInstance = nullptr;

/** @brief 将 libftpsrv 日志回调转发给当前 FtpManager */
static void ftpLogCallback(enum FTP_API_LOG_TYPE type, const char* msg) {
    if (s_activeInstance) s_activeInstance->handleLog(static_cast<int>(type), msg);
}

/**
 * @brief 根据 FTP 运行状态切换 socket 缓冲池配置
 * @param ftpMode 是否启用 FTP 所需的大缓冲池
 *
 * borealis 在 applet 模式下的默认 socket 池不足以支撑多线程 FTP 传输。
 * FTP 启动时切换为大缓冲池，停止时恢复 HTTP 使用的配置，避免常驻内存浪费。
 *
 * socketExit() 会销毁当前所有 socket，包括 nxlink。
 */
static void socketReconfigure(bool ftpMode) {
    http::suspend();
    socketExit();
    if (ftpMode) {
        // 约 96MB，仅在 FTP 服务运行期间使用。
        const SocketInitConfig config = {
            .tcp_tx_buf_size     = 1024 * 64,       // 64KB 初始发送缓冲
            .tcp_rx_buf_size     = 1024 * 64,       // 64KB 初始接收缓冲
            .tcp_tx_buf_max_size = 1024 * 1024 * 4, // 4MB 发送上限
            .tcp_rx_buf_max_size = 1024 * 1024 * 4, // 4MB 接收上限
            .udp_tx_buf_size     = 0x2400,          // libnx 默认 UDP 发送缓冲
            .udp_rx_buf_size     = 0xA500,          // libnx 默认 UDP 接收缓冲
            .sb_efficiency       = 12,              // 可容纳约 12 个全开 socket
            .num_bsd_sessions    = 3,               // FTP poll 模型只需少量会话
            .bsd_service_type    = BsdServiceType_Auto,
        };
        socketInitialize(&config);
    } else {
        // 恢复 http::init() 使用的配置。
        SocketInitConfig cfg = *(socketGetDefaultInitConfig());
        cfg.tcp_rx_buf_max_size = 1024 * 512;
        cfg.num_bsd_sessions = 3;
        cfg.sb_efficiency = 4;
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

    // 将 C++ FtpMount 转为 C FtpVfsMountEntry 传给 VFS 层。
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
    result.port = config.port;

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

void FtpManager::handleLog(int type, const char* msg) {
    if (!m_onEvent) return;

    FtpEventData data;
    bool dispatch = false;

    if (type == FTP_API_LOG_TYPE_RESPONSE) {
        // 220：客户端 TCP 连接成功，服务端发送欢迎消息。
        if (std::strncmp(msg, "220 ", 4) == 0) {
            data.event = FtpEvent::Connected;
            dispatch = true;
        // 221：客户端发送 QUIT，服务端确认关闭连接。
        } else if (std::strncmp(msg, "221 ", 4) == 0) {
            data.event = FtpEvent::Disconnected;
            dispatch = true;
        // 226：STOR 或 RETR 完成；m_transferring 防止把 LIST 的 226 计入传输。
        } else if (std::strncmp(msg, "226 ", 4) == 0 && m_transferring) {
            m_transferring = false;
            m_successCount++;
            data.event = FtpEvent::TransferEnd;
            data.successCount = m_successCount;
            data.failedCount = m_failedCount;
            dispatch = true;
        }
    } else if (type == FTP_API_LOG_TYPE_ERROR) {
        // 426：STOR 或 RETR 传输中断。
        if (std::strncmp(msg, "426 ", 4) == 0 && m_transferring) {
            m_transferring = false;
            m_failedCount++;
            data.event = FtpEvent::TransferFailed;
            data.successCount = m_successCount;
            data.failedCount = m_failedCount;
            dispatch = true;
        }
    } else if (type == FTP_API_LOG_TYPE_COMMAND) {
        // STOR 为上传到 Switch，RETR 为从 Switch 下载。
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
