/**
 * MtpManager - MTP 文件传输模块
 * 薄封装层，管理 libhaze 的启停和事件转发
 * 调用方通过 MtpMount 配置挂载点，通过 EventCallback 接收结构化事件
 */

#pragma once

#include <string>
#include <vector>
#include <functional>

// 挂载配置，调用方传入
struct MtpMount {
    std::string basePath;      // 映射到 SD 卡的目录
    std::string displayName;   // PC 上显示的盘符名
};

// MTP 事件类型，映射 libhaze 的 CallbackType，None 为默认值
enum class MtpEvent {
    None,            // 未设置（默认值）
    Connected,       // PC 连接
    Disconnected,    // PC 断开
    WriteBegin,      // 开始写入文件（PC → Switch）
    WriteProgress,   // 写入进度
    WriteEnd,        // 文件写入完成
    ReadBegin,       // 开始读取文件（Switch → PC）
    ReadProgress,    // 读取进度
    ReadEnd,         // 文件读取完成
    CreateFile,      // 创建文件
    DeleteFile,      // 删除文件
    CreateFolder,    // 创建目录
    DeleteFolder,    // 删除目录
    RenameFile,      // 重命名文件
    RenameFolder,    // 重命名目录
};

// 事件数据，由回调传出（持有字符串副本，可安全跨线程传递）
struct MtpEventData {
    MtpEvent event = MtpEvent::None;
    std::string filename;       // 文件/目录名（所有事件）
    std::string newFilename;    // 仅 Rename 事件：新名称
    long long transferred = 0;   // 仅 Progress 事件：已传输字节数（offset + chunkSize）
    long long fileSize = 0;     // Progress/Begin/End/CreateFile 事件：文件总大小（可能为 0）
};

class MtpManager {
public:
    using EventCallback = std::function<void(const MtpEventData&)>;

    MtpManager() = default;
    ~MtpManager();

    MtpManager(const MtpManager&) = delete;
    MtpManager& operator=(const MtpManager&) = delete;

    /**
     * @brief 启动 MTP 服务，为每个 mount 创建一个 proxy 盘符
     * @param mounts 挂载配置列表
     * @param onEvent 事件回调
     */
    bool start(std::vector<MtpMount> mounts, EventCallback onEvent = nullptr);

    /** @brief 停止 MTP 服务（会等待 MTP 线程结束） */
    void stop();

    /** @brief 是否正在运行 */
    bool isRunning() const;

private:
    bool m_running = false;
};
