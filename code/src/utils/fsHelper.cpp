/**
 * fsHelper - 文件系统操作封装
 * 基于 libnx 原生 API
 */

#include "utils/fsHelper.hpp"
#include <switch.h>
#include <cstring>
#include <algorithm>

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

bool ensureDir(const FsPath& path) {
    if (dirExists(path)) return true;
    FsFileSystem* fs = getSdFs();
    if (!fs) return false;
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

} // namespace fs
