/**
 * MtpManager - MTP 文件传输模块
 * 薄封装层，管理 libhaze 的启停和事件转发
 * 调用方通过 MtpMount 配置挂载点，通过 EventCallback 接收结构化事件
 */

#pragma once

#include <functional>
#include <string>
#include <vector>

/** @brief MTP 挂载配置 */
struct MtpMount {
    std::string basePath;    // 映射到 SD 卡的目录
    std::string displayName; // PC 上显示的盘符名
};

/** @brief MTP 事件类型，映射 libhaze 的 CallbackType */
enum class MtpEvent {
    None,          // 未设置
    Connected,     // PC 连接
    Disconnected,  // PC 断开
    WriteBegin,    // 开始写入文件（PC -> Switch）
    WriteProgress, // 写入进度
    WriteEnd,      // 文件写入完成
    ReadBegin,     // 开始读取文件（Switch -> PC）
    ReadProgress,  // 读取进度
    ReadEnd,       // 文件读取完成
    CreateFile,    // 创建文件
    DeleteFile,    // 删除文件
    CreateFolder,  // 创建目录
    DeleteFolder,  // 删除目录
    RenameFile,    // 重命名文件
    RenameFolder,  // 重命名目录
};

/** @brief MTP 事件数据，持有字符串副本，可安全跨线程传递 */
struct MtpEventData {
    MtpEvent event = MtpEvent::None; // 事件类型
    std::string filename;            // 文件或目录名称
    std::string newFilename;         // Rename 事件的新名称
    long long transferred = 0;       // Progress 事件已传输的字节数
    long long fileSize = 0;          // 文件总字节数，未知时为 0
};

class MtpManager {
public:
    using EventCallback = std::function<void(const MtpEventData&)>;

    /** @brief 创建 MTP 管理器 */
    MtpManager() = default;

    /** @brief 停止仍在运行的 MTP 服务并销毁管理器 */
    ~MtpManager();

    MtpManager(const MtpManager&) = delete;
    MtpManager& operator=(const MtpManager&) = delete;

    /**
     * @brief 启动 MTP 服务，为每个挂载配置创建一个 proxy 盘符
     * @param mounts 挂载配置列表
     * @param onEvent 事件回调
     * @return 是否启动成功
     */
    bool start(std::vector<MtpMount> mounts, EventCallback onEvent = nullptr);

    /** @brief 停止 MTP 服务并等待 MTP 线程结束 */
    void stop();

    /**
     * @brief 获取 MTP 服务是否正在运行
     * @return MTP 服务正在运行时返回 true
     */
    bool isRunning() const;

private:
    bool m_running = false; // MTP 服务是否正在运行
};
