/**
 * FtpManager - FTP 文件传输模块
 * 薄封装层，管理 libftpsrv 的启停
 * 调用方通过 FtpConfig 配置参数，start() 返回 IP 等启动信息
 */

#pragma once

#include <string>
#include <vector>
#include <functional>

// 快捷目录配置（FTP 客户端的根目录展示这些入口）
struct FtpMount {
    std::string basePath;      // SD 卡实际路径（如 "/mods2"）
    std::string displayName;   // FTP 客户端看到的目录名（如 "mods"）
};

struct FtpConfig {
    std::vector<FtpMount> mounts;      // 快捷目录列表（为空时暴露整个 SD 卡）
    std::string user = "";             // FTP 用户名（匿名模式下忽略）
    std::string password = "";         // FTP 密码（匿名模式下忽略）
    unsigned port = 6666;              // FTP 端口
    bool anonymous = true;             // 是否允许匿名访问
};

struct FtpStartResult {
    bool success = false;              // 是否启动成功
    std::string ipAddress;             // Switch 的 IP 地址
    std::string errorMessage;          // 失败时的错误信息
};

// FTP 事件类型（对标 MtpEvent）
enum class FtpEvent {
    Connected,       // 客户端已连接
    Disconnected,    // 客户端已断开（正常 QUIT）
    TransferBegin,   // 开始接收文件
    TransferEnd,     // 文件接收完成（成功）
    TransferFailed,  // 文件接收失败
};

// 事件数据（由回调传出）
struct FtpEventData {
    FtpEvent event = FtpEvent::Connected;
    int successCount = 0;  // 累计成功传输文件数（TransferEnd 时有效）
    int failedCount = 0;   // 累计失败传输文件数（TransferFailed 时有效）
};

class FtpManager {
public:
    using EventCallback = std::function<void(const FtpEventData&)>;

    FtpManager() = default;
    ~FtpManager();

    FtpManager(const FtpManager&) = delete;
    FtpManager& operator=(const FtpManager&) = delete;

    /**
     * @brief 启动 FTP 服务，为每个 mount 创建快捷目录
     * @param config FTP 配置
     * @param onEvent 事件回调
     */
    FtpStartResult start(FtpConfig config, EventCallback onEvent = nullptr);

    /** @brief 停止 FTP 服务（会等待 FTP 线程结束） */
    void stop();

    /** @brief 是否正在运行 */
    bool isRunning() const;

    /**
     * @brief log_callback 桥接入口（FTP 子线程调用，解析事件后派发给 m_onEvent）
     * @param type 日志类型
     * @param msg 日志内容
     */
    void handleLog(int type, const char* msg);

private:
    bool m_running = false;
    EventCallback m_onEvent;          // 事件回调（调用方传入）
    int m_successCount = 0;           // 累计成功传输文件数
    int m_failedCount = 0;            // 累计失败传输文件数
    bool m_transferring = false;      // 当前是否有文件正在传输
};
