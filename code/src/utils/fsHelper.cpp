/**
 * fsHelper - 文件系统操作封装
 * 基于 libnx 原生 API
 */

#include "utils/fsHelper.hpp"
#include "utils/format.hpp"
#include <switch.h>
#include <cstring>
#include <algorithm>
#include <chrono>

namespace fs {

namespace {

    // 获取 SD 卡文件系统句柄（libnx 启动时已挂载，无需关闭）
    inline FsFileSystem* getSdFs() {
        static FsFileSystem* s_fs = fsdevGetDeviceFileSystem("sdmc:");
        return s_fs;
    }

} // namespace

bool dirExists(const FsPath& path) {
    FsFileSystem* fs = getSdFs();
    if (!fs) return false;

    FsDirEntryType type;
    Result rc = fsFsGetEntryType(fs, path, &type);
    if (R_FAILED(rc)) return false;

    return type == FsDirEntryType_Dir;
}

bool fileExists(const FsPath& path) {
    FsFileSystem* fs = getSdFs();
    if (!fs) return false;

    FsDirEntryType type;
    Result rc = fsFsGetEntryType(fs, path, &type);
    if (R_FAILED(rc)) return false;

    return type == FsDirEntryType_File;
}

uint32_t createDir(const FsPath& path) {
    FsFileSystem* fs = getSdFs();
    if (!fs) return -1;
    Result rc = fsFsCreateDirectory(fs, path);
    if (rc == 0x402) return 0;  // already exists
    return rc;
}

bool ensureDir(const FsPath& path) {
    if (dirExists(path)) return true;
    FsFileSystem* fs = getSdFs();
    if (!fs) return false;
    /* 递归确保父目录存在 */
    std::string parent(path);
    auto pos = parent.rfind('/');
    if (pos != 0 && pos != std::string::npos) {
        parent.resize(pos);
        if (!ensureDir(parent)) return false;
    }
    return R_SUCCEEDED(fsFsCreateDirectory(fs, path));
}

std::vector<std::string> listSubDirs(const FsPath& path) {
    std::vector<std::string> result;
    FsFileSystem* fs = getSdFs();
    if (!fs) return result;

    FsDir dir;
    Result rc = fsFsOpenDirectory(fs, path, FsDirOpenMode_ReadDirs, &dir);
    if (R_FAILED(rc)) return result;

    // 先获取条目数量
    s64 count = 0;
    rc = fsDirGetEntryCount(&dir, &count);
    if (R_FAILED(rc) || count <= 0) {
        fsDirClose(&dir);
        return result;
    }

    // 一次性读取所有目录条目
    std::vector<FsDirectoryEntry> entries(count);
    s64 readCount = 0;
    rc = fsDirRead(&dir, &readCount, entries.size(), entries.data());
    fsDirClose(&dir);

    if (R_FAILED(rc)) return result;

    result.reserve(readCount);
    for (s64 i = 0; i < readCount; i++) {
        result.emplace_back(entries[i].name);
    }

    return result;
}

std::vector<std::string> listSubFiles(const FsPath& path, const std::vector<std::string>& exts) {
    std::vector<std::string> result;
    FsFileSystem* fs = getSdFs();
    if (!fs) return result;

    // 以只读文件模式打开目录
    FsDir dir;
    Result rc = fsFsOpenDirectory(fs, path, FsDirOpenMode_ReadFiles, &dir);
    if (R_FAILED(rc)) return result;

    // 获取文件条目数量
    s64 count = 0;
    rc = fsDirGetEntryCount(&dir, &count);
    if (R_FAILED(rc) || count <= 0) {
        fsDirClose(&dir);
        return result;
    }

    // 一次性读取所有文件条目
    std::vector<FsDirectoryEntry> entries(count);
    s64 readCount = 0;
    rc = fsDirRead(&dir, &readCount, entries.size(), entries.data());
    fsDirClose(&dir);
    if (R_FAILED(rc)) return result;

    // 无过滤：返回所有文件
    result.reserve(readCount);
    if (exts.empty()) {
        for (s64 i = 0; i < readCount; i++)
            result.emplace_back(entries[i].name);
        return result;
    }

    // 有过滤：只返回匹配后缀的文件（转小写比较）
    for (s64 i = 0; i < readCount; i++) {
        std::string name(entries[i].name);
        std::transform(name.begin(), name.end(), name.begin(),
            [](unsigned char c) { return std::tolower(c); });
        for (const auto& ext : exts) {
            if (name.size() >= ext.size() &&
                name.compare(name.size() - ext.size(), ext.size(), ext) == 0) {
                result.emplace_back(entries[i].name);
                break;
            }
        }
    }
    return result;
}

// 非严格遍历：目录打开或读取失败时跳过当前目录并继续，不因单次错误中断整个检查。
bool containsFileRecursive(const FsPath& path, const std::vector<std::string>& ignoredNames) {
    std::vector<std::string> stack{std::string(path)};
    std::vector<DirEntry> entries;

    while (!stack.empty()) {
        std::string currentPath = std::move(stack.back());
        stack.pop_back();

        DirReader reader;
        if (reader.open(currentPath) != 0) continue;

        while (reader.read(entries) == 0 && !entries.empty()) {
            for (const auto& entry : entries) {
                if (entry.isFile) {
                    if (std::find(ignoredNames.begin(), ignoredNames.end(), entry.name) == ignoredNames.end()) return true;
                } else {
                    stack.push_back(currentPath + "/" + entry.name);
                }
            }
        }
    }

    return false;
}

int countDirs(const FsPath& path) {
    FsFileSystem* fs = getSdFs();
    if (!fs) return 0;

    FsDir dir;
    Result rc = fsFsOpenDirectory(fs, path, FsDirOpenMode_ReadDirs, &dir);
    if (R_FAILED(rc)) return 0;

    s64 count = 0;
    fsDirGetEntryCount(&dir, &count);
    fsDirClose(&dir);
    return static_cast<int>(count);
}

int countItems(const FsPath& path, const std::vector<std::string>& exts) {
    FsFileSystem* fs = getSdFs();
    if (!fs) return 0;

    // 快速路径：不过滤，一次拿总数
    if (exts.empty()) {
        FsDir dir;
        Result rc = fsFsOpenDirectory(fs, path,
            FsDirOpenMode_ReadDirs | FsDirOpenMode_ReadFiles | FsDirOpenMode_NoFileSize, &dir);
        if (R_FAILED(rc)) return 0;

        s64 count = 0;
        fsDirGetEntryCount(&dir, &count);
        fsDirClose(&dir);
        return static_cast<int>(count);
    }

    // 合并路径：一次打开，内存中区分目录和文件
    FsDir dir;
    Result rc = fsFsOpenDirectory(fs, path,
        FsDirOpenMode_ReadDirs | FsDirOpenMode_ReadFiles | FsDirOpenMode_NoFileSize, &dir);
    if (R_FAILED(rc)) return 0;

    s64 total = 0;
    fsDirGetEntryCount(&dir, &total);
    if (total <= 0) {
        fsDirClose(&dir);
        return 0;
    }

    std::vector<FsDirectoryEntry> entries(total);
    s64 readCount = 0;
    rc = fsDirRead(&dir, &readCount, entries.size(), entries.data());
    fsDirClose(&dir);
    if (R_FAILED(rc)) return 0;

    int count = 0;
    for (s64 i = 0; i < readCount; i++) {
        if (entries[i].type == FsDirEntryType_Dir) { 
            count++; 
            continue; 
        }

        std::string name(entries[i].name);
        std::transform(name.begin(), name.end(), name.begin(),[](unsigned char c) { return std::tolower(c); });

        for (const auto& ext : exts) {
            if (name.size() >= ext.size() && name.compare(name.size() - ext.size(), ext.size(), ext) == 0) {
                count++;
                break;
            }
        }
    }

    return count;
}

std::vector<DirEntry> listItems(const FsPath& path, const std::vector<std::string>& exts) {
    FsFileSystem* fs = getSdFs();
    if (!fs) return {};

    FsDir dir;
    Result rc = fsFsOpenDirectory(fs, path,
        FsDirOpenMode_ReadDirs | FsDirOpenMode_ReadFiles | FsDirOpenMode_NoFileSize, &dir);
    if (R_FAILED(rc)) return {};

    s64 total = 0;
    fsDirGetEntryCount(&dir, &total);
    if (total <= 0) {
        fsDirClose(&dir);
        return {};
    }

    std::vector<FsDirectoryEntry> entries(total);
    s64 readCount = 0;
    rc = fsDirRead(&dir, &readCount, entries.size(), entries.data());
    fsDirClose(&dir);
    if (R_FAILED(rc)) return {};

    std::vector<DirEntry> result;
    result.reserve(readCount);

    for (s64 i = 0; i < readCount; i++) {
        bool isFile = entries[i].type != FsDirEntryType_Dir;

        if (isFile && !exts.empty()) {
            std::string name(entries[i].name);
            std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return std::tolower(c); });

            bool matched = false;
            for (const auto& ext : exts) {
                if (name.size() >= ext.size() && name.compare(name.size() - ext.size(), ext.size(), ext) == 0) {
                    matched = true;
                    break;
                }
            }
            if (!matched) continue;
        }

        result.push_back({std::string(entries[i].name), isFile});
    }

    return result;
}

int64_t getFileSize(const FsPath& path) {
    FsFileSystem* fs = getSdFs();
    if (!fs) return -1;

    FsFile file;
    Result rc = fsFsOpenFile(fs, path, FsOpenMode_Read, &file);
    if (R_FAILED(rc)) return -1;

    s64 size = 0;
    rc = fsFileGetSize(&file, &size);
    fsFileClose(&file);
    return R_SUCCEEDED(rc) ? size : -1;
}

int64_t calcDirSize(const FsPath& path, std::stop_token* token) {
    FsFileSystem* sdFs = getSdFs();
    if (!sdFs) return -1;

    int64_t totalSize = 0;
    std::vector<std::string> dirStack;
    dirStack.push_back(std::string(path));

    FsDirectoryEntry batch[64];

    while (!dirStack.empty()) {
        if (token && token->stop_requested()) return -1;
        std::string curPath = std::move(dirStack.back());
        dirStack.pop_back();

        FsDir dir;
        Result rc = fsFsOpenDirectory(sdFs, FsPath(curPath),
            FsDirOpenMode_ReadDirs | FsDirOpenMode_ReadFiles, &dir);
        if (R_FAILED(rc)) {
            if (dirStack.empty() && totalSize == 0) return -1;
            continue;
        }

        s64 readCount = 0;

        do {
            rc = fsDirRead(&dir, &readCount, 64, batch);
            if (R_FAILED(rc)) break;

            for (s64 i = 0; i < readCount; i++) {
                if (batch[i].type == FsDirEntryType_File) totalSize += batch[i].file_size;
                else dirStack.push_back(curPath + "/" + batch[i].name);
            }
        } while (readCount > 0);

        fsDirClose(&dir);
    }

    return totalSize;
}

bool removeDirAll(const FsPath& path) {
    FsFileSystem* fs = getSdFs();
    if (!fs) return false;
    return R_SUCCEEDED(fsFsDeleteDirectoryRecursively(fs, path));
}

bool removeDirContents(const FsPath& path) {
    FsFileSystem* fs = getSdFs();
    if (!fs) return false;
    return R_SUCCEEDED(fsFsCleanDirectoryRecursively(fs, path));
}

RemoveResult removeDirContentsWithProgress(
    const FsPath& path,
    std::stop_token token,
    std::function<void(int deleted, int total, const char* fileName)> onProgress)
{
    using Clock = std::chrono::steady_clock;
    auto startTime = Clock::now();

    FsFileSystem* sdFs = getSdFs();
    if (!sdFs) return {RemoveResult::FsError, 0, 0, std::string(path), ""};

    constexpr auto THROTTLE_INTERVAL = std::chrono::milliseconds(100);

    // ── 阶段一：扫描 ──
    // DFS 栈遍历整棵目录树，只读不写
    // 每个目录收集为一个 DirCollection（路径 + 轻量条目列表）
    // 同时统计文件总数 fileTotal，供进度显示

    std::vector<DirCollection> collections;
    std::vector<std::string> dirStack;
    dirStack.push_back(std::string(path));
    int fileTotal = 0;
    auto lastUpdate = Clock::now();

    // 栈上缓冲区，每次从目录批量读取最多 64 个条目
    FsDirectoryEntry batch[64];

    while (!dirStack.empty()) {
        if (token.stop_requested()) return {RemoveResult::Cancelled, 0, fileTotal, "", ""};

        std::string curPath = std::move(dirStack.back());
        dirStack.pop_back();

        // 打开当前目录，同时读取子目录和文件
        FsDir dir;
        Result rc = fsFsOpenDirectory(sdFs, FsPath(curPath), FsDirOpenMode_ReadDirs | FsDirOpenMode_ReadFiles, &dir);
        if (R_FAILED(rc)) return {RemoveResult::FsError, 0, fileTotal, curPath, format::resultHex(rc)};

        DirCollection collection;
        collection.path = curPath;

        // 分批读取目录内容，直到 readCount == 0 表示读完
        s64 readCount = 0;
        do {
            rc = fsDirRead(&dir, &readCount, 64, batch);
            if (R_FAILED(rc)) {
                fsDirClose(&dir);
                return {RemoveResult::FsError, 0, fileTotal, curPath, format::resultHex(rc)};
            }

            for (s64 i = 0; i < readCount; i++) {
                bool isFile = (batch[i].type == FsDirEntryType_File);
                // 只保留文件名和类型，不存 FsDirectoryEntry（784 字节/个太大）
                collection.entries.push_back({batch[i].name, isFile});
                if (isFile)
                    fileTotal++;
                else
                    dirStack.push_back(curPath + "/" + batch[i].name);
            }
        } while (readCount > 0);

        fsDirClose(&dir);
        collections.push_back(std::move(collection));

        // 扫描阶段回调：deleted=0, total=当前已扫描文件数, fileName=nullptr
        if (onProgress) {
            auto now = Clock::now();
            if (now - lastUpdate >= THROTTLE_INTERVAL) {
                onProgress(0, fileTotal, nullptr);
                lastUpdate = now;
            }
        }
    }

    // ── 阶段二：删除 ──
    // 逆序遍历 collections，保证子目录先于父目录处理
    // （阶段一中父目录总是在子目录之前被加入 collections）
    // 每个 collection 内：先删所有文件，再删所有子目录（此时已空）
    // root 目录本身不会被删除（它只是 collection 的 path，不是任何 entry）

    int fileDeleted = 0;
    lastUpdate = Clock::now();

    for (int ci = static_cast<int>(collections.size()) - 1; ci >= 0; ci--) {
        auto& collection = collections[ci];

        // 先删文件，逐个 IPC 调用
        for (auto& entry : collection.entries) {
            if (!entry.isFile) continue;
            if (token.stop_requested())
                return {RemoveResult::Cancelled, fileDeleted, fileTotal, "", ""};

            std::string fullPath = collection.path + "/" + entry.name;
            Result rc = fsFsDeleteFile(sdFs, FsPath(fullPath));
            if (R_FAILED(rc))
                return {RemoveResult::FsError, fileDeleted, fileTotal, fullPath, format::resultHex(rc)};

            fileDeleted++;
            svcSleepThread(100000ULL);  // 0.1ms 让出 CPU（参考 sphaira）

            // 删除阶段回调：deleted=已删数, total=总数, fileName=当前文件名
            if (onProgress) {
                auto now = Clock::now();
                if (now - lastUpdate >= THROTTLE_INTERVAL || fileDeleted == fileTotal) {
                    onProgress(fileDeleted, fileTotal, entry.name.c_str());
                    lastUpdate = now;
                }
            }
        }

        // 文件删完后，删除该 collection 下的子目录（逆序保证此时已空）
        for (auto& entry : collection.entries) {
            if (entry.isFile) continue;
            std::string fullPath = collection.path + "/" + entry.name;
            Result rc = fsFsDeleteDirectory(sdFs, FsPath(fullPath));
            if (R_FAILED(rc))
                return {RemoveResult::FsError, fileDeleted, fileTotal, fullPath, format::resultHex(rc)};
        }
    }

    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - startTime).count();
    return {RemoveResult::Completed, fileDeleted, fileTotal, "", "", format::elapsed(elapsedMs)};
}

bool moveDir(const FsPath& src, const FsPath& dest) {
    FsFileSystem* fs = getSdFs();
    if (!fs) return false;
    return R_SUCCEEDED(fsFsRenameDirectory(fs, src, dest));
}

bool moveFile(const FsPath& src, const FsPath& dest) {
    FsFileSystem* fs = getSdFs();
    if (!fs) return false;
    return R_SUCCEEDED(fsFsRenameFile(fs, src, dest));
}

std::string ensureUniqueDirPath(const std::string& path) {
    if (!dirExists(path)) return path;

    for (int i = 1; ; i++) {
        std::string candidate = path + "_" + std::to_string(i);
        if (!dirExists(candidate)) return candidate;
    }
}

bool deleteFile(const FsPath& path) {
    FsFileSystem* fs = getSdFs();
    if (!fs) return false;
    return R_SUCCEEDED(fsFsDeleteFile(fs, path));
}

bool deleteEmptyDir(const FsPath& path) {
    FsFileSystem* fs = getSdFs();
    if (!fs) return false;
    return R_SUCCEEDED(fsFsDeleteDirectory(fs, path));
}

uint32_t writeFile(const FsPath& path, const void* data, size_t size) {
    FsFileSystem* fs = getSdFs();
    if (!fs) return -1;

    Result createRc = fsFsCreateFile(fs, path, static_cast<s64>(size), 0);
    if (R_FAILED(createRc) && createRc != 0x402) return createRc;

    FsFile file;
    Result rc = fsFsOpenFile(fs, path, FsOpenMode_Write, &file);
    if (R_FAILED(rc)) return rc;

    if (createRc == 0x402) fsFileSetSize(&file, static_cast<s64>(size));
    rc = fsFileWrite(&file, 0, data, size, FsWriteOption_None);
    fsFileClose(&file);
    return rc;
}

std::vector<uint8_t> readFile(const FsPath& path) {
    FileReader reader;
    if (reader.open(path) != 0) return {};

    int64_t fileSize = reader.size();
    if (fileSize <= 0) return {};

    std::vector<uint8_t> data(static_cast<size_t>(fileSize));
    if (reader.read(data.data(), data.size()) != data.size()) return {};

    return data;
}

FileReader::~FileReader() {
    if (m_open) fsFileClose(&m_handle);
}

uint32_t FileReader::open(const FsPath& path, int64_t fileSize) {
    if (m_open) {
        fsFileClose(&m_handle);
        m_open = false;
        m_offset = 0;
        m_size = 0;
    }

    FsFileSystem* sdFs = getSdFs();
    if (!sdFs) return -1;

    Result rc = fsFsOpenFile(sdFs, path, FsOpenMode_Read, &m_handle);
    if (R_FAILED(rc)) return rc;

    if (fileSize >= 0) {
        m_size = fileSize;
    } else {
        rc = fsFileGetSize(&m_handle, &m_size);
        if (R_FAILED(rc)) {
            fsFileClose(&m_handle);
            return rc;
        }
    }

    m_open = true;
    return 0;
}

size_t FileReader::read(void* buf, size_t bufSize) {
    if (!m_open || m_offset >= m_size) return 0;

    u64 bytesRead = 0;
    Result rc = fsFileRead(&m_handle, m_offset, buf, bufSize, FsReadOption_None, &bytesRead);
    if (R_FAILED(rc)) return 0;

    m_offset += static_cast<int64_t>(bytesRead);
    return static_cast<size_t>(bytesRead);
}

FileWriter::~FileWriter() {
    if (m_open) fsFileClose(&m_handle);
}

uint32_t FileWriter::open(const FsPath& path, int64_t fileSize) {
    if (m_open) { 
        fsFileClose(&m_handle); 
        m_open = false; 
        m_offset = 0; 
    }

    FsFileSystem* sdFs = getSdFs();
    if (!sdFs) return -1;

    Result createRc = fsFsCreateFile(sdFs, path, fileSize, 0);
    if (R_FAILED(createRc) && createRc != 0x402) return createRc;

    Result rc = fsFsOpenFile(sdFs, path, FsOpenMode_Write, &m_handle);
    if (R_FAILED(rc)) return rc;

    if (createRc == 0x402) {
        rc = fsFileSetSize(&m_handle, fileSize);
        if (R_FAILED(rc)) { 
            fsFileClose(&m_handle); 
            return rc; 
        }
    }

    m_open = true;
    return 0;
}

uint32_t FileWriter::write(const void* data, size_t size) {
    if (!m_open) return -1;
    Result rc = fsFileWrite(&m_handle, m_offset, data, size, FsWriteOption_None);
    if (R_SUCCEEDED(rc)) m_offset += static_cast<int64_t>(size);
    return rc;
}

DirReader::~DirReader() {
    if (m_open) fsDirClose(&m_handle);
}

uint32_t DirReader::open(const FsPath& path) {
    if (m_open) {
        fsDirClose(&m_handle);
        m_open = false;
    }

    FsFileSystem* sdFs = getSdFs();
    if (!sdFs) return -1;

    Result rc = fsFsOpenDirectory(sdFs, path, FsDirOpenMode_ReadDirs | FsDirOpenMode_ReadFiles, &m_handle);
    if (R_FAILED(rc)) return rc;

    m_open = true;
    return 0;
}

uint32_t DirReader::read(std::vector<DirEntry>& out, int batchSize) {
    out.clear();
    if (!m_open) return -1;

    std::vector<FsDirectoryEntry> batch(batchSize);
    s64 readCount = 0;

    Result rc = fsDirRead(&m_handle, &readCount, batch.size(), batch.data());
    if (R_FAILED(rc)) return rc;

    out.reserve(readCount);
    for (s64 i = 0; i < readCount; ++i) {
        bool isFile = batch[i].type != FsDirEntryType_Dir;
        out.push_back({batch[i].name, isFile, isFile ? batch[i].file_size : 0});
    }
    return 0;
}

void DirReader::close() {
    if (m_open) {
        fsDirClose(&m_handle);
        m_open = false;
    }
}

} // namespace fs
