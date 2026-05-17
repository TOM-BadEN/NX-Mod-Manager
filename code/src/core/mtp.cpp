#include "core/mtp.hpp"
#include "utils/fsHelper.hpp"
#include <haze.h>
#include <cstring>
#include <cstdio>

// ── 模块级状态（.cpp 内部，header 完全看不到） ──

static bool s_active = false;                   // 是否已有实例在运行（防止多实例）
static MtpManager::EventCallback s_onEvent;     // 调用方注册的事件回调
static haze::FsEntries s_fsEntries;             // libhaze 要求的 proxy 列表
static long long s_pendingFileSize = 0;         // proxy CreateFile 捕获的文件大小，供回调使用

// ── MtpProxy（内部类，不暴露给外部） ──
// 实现 libhaze 的 FileSystemProxyImpl 接口
// 每个方法将 MTP 相对路径拼上 basePath，转发给 Switch SD 卡文件系统

class MtpProxy : public haze::FileSystemProxyImpl {
public:
    // basePath: SD 卡目录（如 "/mods2/!temp_mods/!temp_MTP"）
    // displayName: PC 上看到的盘符名
    // name: libhaze 内部标识（第一个 proxy 为空串，后续为 "1","2",...）
    MtpProxy(std::string basePath, std::string displayName, std::string name)
        : m_basePath(std::move(basePath))
        , m_displayName(std::move(displayName))
        , m_name(std::move(name))
        , m_fileSystem(fsdevGetDeviceFileSystem("sdmc"))
    {
        // 去掉尾部斜杠（"/" 本身除外），避免拼路径时出现双斜杠
        if (m_basePath.size() > 1 && m_basePath.back() == '/') {
            m_basePath.pop_back();
        }
        // 确保 basePath 目录存在
        fs::ensureDir(m_basePath);
    }

    // SD 卡文件系统是否可用（构造后检查）
    bool isValid() const { return m_fileSystem != nullptr; }

    // libhaze 内部用的存储 ID
    const char* GetName() const override { return m_name.c_str(); }
    // PC 上显示的盘符名
    const char* GetDisplayName() const override { return m_displayName.c_str(); }

    Result GetTotalSpace(const char* path, s64* out) override {
        char fullPath[FS_MAX_PATH];
        buildFullPath(path, fullPath);
        return fsFsGetTotalSpace(m_fileSystem, fullPath, out);
    }

    Result GetFreeSpace(const char* path, s64* out) override {
        char fullPath[FS_MAX_PATH];
        buildFullPath(path, fullPath);
        return fsFsGetFreeSpace(m_fileSystem, fullPath, out);
    }

    Result GetEntryType(const char* path, FsDirEntryType* outEntryType) override {
        char fullPath[FS_MAX_PATH];
        buildFullPath(path, fullPath);
        return fsFsGetEntryType(m_fileSystem, fullPath, outEntryType);
    }

    // size 是 PC 告知的文件总大小（现代 Windows 走 SendObjectPropList 时有值，旧客户端可能为 0）
    // 先保存到 s_pendingFileSize 供回调使用，再传 size=0 创建文件避免预分配阻塞
    Result CreateFile(const char* path, s64 size, u32 option) override {
        char fullPath[FS_MAX_PATH];
        buildFullPath(path, fullPath);
        s_pendingFileSize = size;
        return fsFsCreateFile(m_fileSystem, fullPath, 0, option);
    }

    Result DeleteFile(const char* path) override {
        char fullPath[FS_MAX_PATH];
        buildFullPath(path, fullPath);
        return fsFsDeleteFile(m_fileSystem, fullPath);
    }

    Result RenameFile(const char* oldPath, const char* newPath) override {
        char fullOldPath[FS_MAX_PATH];
        char fullNewPath[FS_MAX_PATH];
        buildFullPath(oldPath, fullOldPath);
        buildFullPath(newPath, fullNewPath);
        return fsFsRenameFile(m_fileSystem, fullOldPath, fullNewPath);
    }

    Result OpenFile(const char* path, u32 mode, FsFile* outFile) override {
        char fullPath[FS_MAX_PATH];
        buildFullPath(path, fullPath);
        return fsFsOpenFile(m_fileSystem, fullPath, mode, outFile);
    }

    // libhaze 在读取文件前会调用 GetFileSize，顺手捕获文件大小供 Read 回调使用
    // 写入路径的 s_pendingFileSize 由 CreateFile 设置，两者不冲突（MTP 单线程串行）
    Result GetFileSize(FsFile* file, s64* outSize) override {
        Result rc = fsFileGetSize(file, outSize);
        s_pendingFileSize = *outSize;
        return rc;
    }

    Result SetFileSize(FsFile* file, s64 size) override {
        return fsFileSetSize(file, size);
    }

    Result ReadFile(FsFile* file, s64 offset, void* buffer, u64 readSize, u32 option, u64* outBytesRead) override {
        return fsFileRead(file, offset, buffer, readSize, option, outBytesRead);
    }

    Result WriteFile(FsFile* file, s64 offset, const void* buffer, u64 writeSize, u32 option) override {
        return fsFileWrite(file, offset, buffer, writeSize, option);
    }

    void CloseFile(FsFile* file) override {
        fsFileClose(file);
    }

    Result CreateDirectory(const char* path) override {
        char fullPath[FS_MAX_PATH];
        buildFullPath(path, fullPath);
        return fsFsCreateDirectory(m_fileSystem, fullPath);
    }

    Result DeleteDirectoryRecursively(const char* path) override {
        char fullPath[FS_MAX_PATH];
        buildFullPath(path, fullPath);
        return fsFsDeleteDirectoryRecursively(m_fileSystem, fullPath);
    }

    Result RenameDirectory(const char* oldPath, const char* newPath) override {
        char fullOldPath[FS_MAX_PATH];
        char fullNewPath[FS_MAX_PATH];
        buildFullPath(oldPath, fullOldPath);
        buildFullPath(newPath, fullNewPath);
        return fsFsRenameDirectory(m_fileSystem, fullOldPath, fullNewPath);
    }

    Result OpenDirectory(const char* path, u32 mode, FsDir* outDir) override {
        char fullPath[FS_MAX_PATH];
        buildFullPath(path, fullPath);
        return fsFsOpenDirectory(m_fileSystem, fullPath, mode, outDir);
    }

    Result ReadDirectory(FsDir* dir, s64* outTotalEntries, size_t maxEntries, FsDirectoryEntry* buffer) override {
        return fsDirRead(dir, outTotalEntries, maxEntries, buffer);
    }

    Result GetDirectoryEntryCount(FsDir* dir, s64* outCount) override {
        return fsDirGetEntryCount(dir, outCount);
    }

    void CloseDirectory(FsDir* dir) override {
        fsDirClose(dir);
    }

private:
    std::string m_basePath;         // SD 卡目录（无尾部斜杠）
    std::string m_displayName;      // PC 盘符名
    std::string m_name;             // libhaze 内部 ID
    FsFileSystem* m_fileSystem;     // SD 卡文件系统句柄

    // libhaze 的 FileSystemProxy wrapper 会 FixPath（去掉一个前导 /）
    // 但第一个 proxy（name=""）的对象名以 "//" 开头，FixPath 只去一个，结果仍有前导 /
    // 所以这里先 strip relativePath 的前导 /，避免拼出双斜杠路径
    void buildFullPath(const char* relativePath, char* output) {
        if (std::strcmp(relativePath, "/") == 0 || relativePath[0] == '\0') {
            std::snprintf(output, FS_MAX_PATH, "%s", m_basePath.c_str());
            return;
        }
        const char* relative = (relativePath[0] == '/') ? relativePath + 1 : relativePath;
        if (m_basePath.size() == 1 && m_basePath[0] == '/') {
            std::snprintf(output, FS_MAX_PATH, "/%s", relative);
            return;
        }
        std::snprintf(output, FS_MAX_PATH, "%s/%s", m_basePath.c_str(), relative);
    }
};

// ── haze 回调 ──
// libhaze 在 MTP 线程上调用，把 CallbackType 转换为 MtpEvent 后转发给调用方

static void hazeCallback(const haze::CallbackData* data) {
    if (!s_onEvent || !data) return;

    MtpEventData eventData{};

    switch (data->type) {
        case haze::CallbackType_OpenSession:
            eventData.event = MtpEvent::Connected;
            break;
        case haze::CallbackType_CloseSession:
            eventData.event = MtpEvent::Disconnected;
            break;

        // Write 系列：PC → Switch 传文件
        // s_pendingFileSize 在 proxy CreateFile 时写入，WriteEnd 时清零
        case haze::CallbackType_WriteBegin:
            eventData.event = MtpEvent::WriteBegin;
            eventData.filename = data->file.filename;
            eventData.fileSize = s_pendingFileSize;
            break;
        case haze::CallbackType_WriteProgress:
            eventData.event = MtpEvent::WriteProgress;
            eventData.transferred = data->progress.offset + data->progress.size;
            eventData.fileSize = s_pendingFileSize;
            break;
        case haze::CallbackType_WriteEnd:
            eventData.event = MtpEvent::WriteEnd;
            eventData.filename = data->file.filename;
            eventData.fileSize = s_pendingFileSize;
            s_pendingFileSize = 0;
            break;

        // Read 系列：Switch → PC 传文件
        // s_pendingFileSize 在 proxy GetFileSize 时写入，ReadEnd 时清零
        case haze::CallbackType_ReadBegin:
            eventData.event = MtpEvent::ReadBegin;
            eventData.filename = data->file.filename;
            eventData.fileSize = s_pendingFileSize;
            break;
        case haze::CallbackType_ReadProgress:
            eventData.event = MtpEvent::ReadProgress;
            eventData.transferred = data->progress.offset + data->progress.size;
            eventData.fileSize = s_pendingFileSize;
            break;
        case haze::CallbackType_ReadEnd:
            eventData.event = MtpEvent::ReadEnd;
            eventData.filename = data->file.filename;
            eventData.fileSize = s_pendingFileSize;
            s_pendingFileSize = 0;
            break;

        // 文件/目录操作
        case haze::CallbackType_CreateFile:
            eventData.event = MtpEvent::CreateFile;
            eventData.filename = data->file.filename;
            eventData.fileSize = s_pendingFileSize;
            break;
        case haze::CallbackType_DeleteFile:
            eventData.event = MtpEvent::DeleteFile;
            eventData.filename = data->file.filename;
            break;
        case haze::CallbackType_CreateFolder:
            eventData.event = MtpEvent::CreateFolder;
            eventData.filename = data->file.filename;
            break;
        case haze::CallbackType_DeleteFolder:
            eventData.event = MtpEvent::DeleteFolder;
            eventData.filename = data->file.filename;
            break;
        // Rename 事件用 rename 结构（包含旧名 + 新名）
        case haze::CallbackType_RenameFile:
            eventData.event = MtpEvent::RenameFile;
            eventData.filename = data->rename.filename;
            eventData.newFilename = data->rename.newname;
            break;
        case haze::CallbackType_RenameFolder:
            eventData.event = MtpEvent::RenameFolder;
            eventData.filename = data->rename.filename;
            eventData.newFilename = data->rename.newname;
            break;
        default:
            return;
    }

    s_onEvent(eventData);
}

// ── MtpManager ──

MtpManager::~MtpManager() {
    if (m_running) {
        stop();
    }
}

bool MtpManager::start(std::vector<MtpMount> mounts, EventCallback onEvent) {
    if (m_running) return false;
    if (s_active) return false;
    if (mounts.empty()) return false;

    for (const auto& mount : mounts) {
        if (mount.basePath.empty()) return false;
    }

    s_onEvent = std::move(onEvent);
    s_pendingFileSize = 0;
    s_fsEntries.clear();

    // 第一个 proxy name=""（默认存储），后续 name="1","2",...
    for (size_t i = 0; i < mounts.size(); i++) {
        std::string name = (i == 0) ? "" : std::to_string(i);
        auto proxy = std::make_shared<MtpProxy>(
            std::move(mounts[i].basePath),
            std::move(mounts[i].displayName),
            std::move(name)
        );
        if (!proxy->isValid()) {
            s_fsEntries.clear();
            s_onEvent = nullptr;
            return false;
        }
        s_fsEntries.emplace_back(std::move(proxy));
    }

    // 0x20 = 线程优先级，2 = CPU 核心号（参考 sphaira）
    if (!haze::Initialize(hazeCallback, 0x20, 2, s_fsEntries)) {
        s_fsEntries.clear();
        s_onEvent = nullptr;
        return false;
    }

    s_active = true;
    m_running = true;
    return true;
}

void MtpManager::stop() {
    if (!m_running) return;

    // haze::Exit() 会等待 MTP 线程结束，之后不会再有回调
    haze::Exit();

    s_active = false;
    m_running = false;
    s_fsEntries.clear();
    s_onEvent = nullptr;
    s_pendingFileSize = 0;
}

bool MtpManager::isRunning() const {
    return m_running;
}
