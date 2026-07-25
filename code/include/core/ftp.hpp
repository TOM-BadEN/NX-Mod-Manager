/**
 * FtpManager - FTP 文件传输模块
 * 薄封装层，管理 libftpsrv 的启停
 * 调用方通过 FtpConfig 配置参数，start() 返回 IP 等启动信息
 */

#pragma once

#include <functional>
#include <string>
#include <vector>

/** @brief FTP 快捷目录配置 */
struct FtpMount {
    std::string basePath;    // SD 卡实际路径
    std::string displayName; // FTP 客户端看到的目录名
};

/** @brief FTP 服务配置 */
struct FtpConfig {
    std::vector<FtpMount> mounts; // 快捷目录列表，为空时暴露整个 SD 卡
    std::string user = "";        // FTP 用户名，匿名模式下忽略
    std::string password = "";    // FTP 密码，匿名模式下忽略
    unsigned port = 5000;         // FTP 端口
    bool anonymous = true;        // 是否允许匿名访问
};

/** @brief FTP 服务启动结果 */
struct FtpStartResult {
    bool success = false;     // 是否启动成功
    std::string ipAddress;    // Switch 的 IP 地址
    unsigned port = 0;        // FTP 服务实际使用的端口
    std::string errorMessage; // 失败时的错误信息
};

/** @brief FTP 事件类型 */
enum class FtpEvent {
    Connected,      // 客户端已连接
    Disconnected,   // 客户端已正常断开
    TransferBegin,  // 开始传输文件
    TransferEnd,    // 文件传输完成
    TransferFailed, // 文件传输失败
};

/** @brief FTP 事件数据 */
struct FtpEventData {
    FtpEvent event = FtpEvent::Connected; // 事件类型
    int successCount = 0;                 // 累计成功传输文件数
    int failedCount = 0;                  // 累计失败传输文件数
};

class FtpManager {
public:
    using EventCallback = std::function<void(const FtpEventData&)>;

    /** @brief 创建 FTP 管理器 */
    FtpManager() = default;

    /** @brief 停止仍在运行的 FTP 服务并销毁管理器 */
    ~FtpManager();

    FtpManager(const FtpManager&) = delete;
    FtpManager& operator=(const FtpManager&) = delete;

    /**
     * @brief 启动 FTP 服务，为每个挂载配置创建快捷目录
     * @param config FTP 配置
     * @param onEvent 事件回调
     * @return FTP 服务启动结果
     */
    FtpStartResult start(FtpConfig config, EventCallback onEvent = nullptr);

    /** @brief 停止 FTP 服务并等待 FTP 线程结束 */
    void stop();

    /**
     * @brief 获取 FTP 服务是否正在运行
     * @return FTP 服务正在运行时返回 true
     */
    bool isRunning() const;

    /**
     * @brief 解析 FTP 子线程日志并派发事件
     * @param type 日志类型
     * @param msg 日志内容
     */
    void handleLog(int type, const char* msg);

private:
    bool m_running = false;        // FTP 服务是否正在运行
    EventCallback m_onEvent;       // 调用方传入的事件回调
    int m_successCount = 0;        // 累计成功传输文件数
    int m_failedCount = 0;         // 累计失败传输文件数
    bool m_transferring = false;   // 当前是否有文件正在传输
};
