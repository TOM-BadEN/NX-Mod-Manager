/**
 * ModInstaller - 目录模组安装与卸载实现
 */

#include "core/modInstaller/installDir.hpp"
#include "core/modInstaller/utils.hpp"
#include "core/modInstaller/specialRules.hpp"
#include "utils/fsHelper.hpp"
#include "utils/crc32.hpp"
#include "utils/format.hpp"
#include "core/modInstaller/modFileRefCount.hpp"
#include <borealis/core/i18n.hpp>
#include "common/config.hpp"
#include <algorithm>

namespace ModInstaller {

namespace {

constexpr int64_t smallFileThreshold = 8 * 1024 * 1024;   // 8MB
constexpr size_t cacheMemLimit = 32 * 1024 * 1024;        // 32MB

struct DirModFile {
    std::string sourcePath;
    std::string targetPath;   // pchtxt 为空，安装时由转换结果决定
    int64_t size;
    bool skip = false;
};

struct DirScanResult {
    bool success = false;
    std::string errorPath;
    std::string errorMsg;
    std::vector<std::string> dirs;
    std::vector<DirModFile> files;
};

DirScanResult scanDirMod(const std::string& modPath, const std::string& tid, bool skipDotEntries,
    std::function<void(const Progress&)> progressCb, std::stop_token token) {
    DirScanResult result;

    std::vector<std::string> rawDirs;
    std::vector<std::string> stack;
    stack.push_back(modPath);
    const size_t baseLen = modPath.size();

    while (!stack.empty()) {
        if (token.stop_requested()) return result;

        std::string curPath = std::move(stack.back());
        stack.pop_back();

        fs::DirReader reader;
        uint32_t openRc = reader.open(curPath);
        if (openRc != 0) {
            result.errorPath = curPath;
            result.errorMsg = brls::getStr("other/installer/openDirFailed", format::resultHex(openRc));
            return result;
        }

        std::vector<fs::DirEntry> batch;
        while (true) {
            uint32_t readRc = reader.read(batch);
            if (readRc != 0) {
                result.errorPath = curPath;
                result.errorMsg = brls::getStr("other/installer/readDirFailed", format::resultHex(readRc));
                return result;
            }
            if (batch.empty()) break;

            for (auto& e : batch) {
                if (token.stop_requested()) return result;
                if (skipDotEntries && !e.name.empty() && e.name[0] == '.') continue;

                std::string fullPath = curPath + "/" + e.name;
                std::string relPath = fullPath.substr(baseLen + 1);

                if (!e.isFile) {
                    rawDirs.push_back(relPath);
                    stack.push_back(std::move(fullPath));
                    continue;
                }

                if (utils::endsWith(relPath, pchtxtExt)) {
                    result.files.push_back({std::move(fullPath), {}, e.fileSize});
                    int totalFiles = static_cast<int>(result.files.size());
                    if (progressCb && (totalFiles % 1000 == 0)) progressCb({false, 0, totalFiles, brls::getStr("other/installer/scanningFiles"), 0, 0});
                    continue;
                }

                std::string target = utils::buildTargetPath(relPath, tid);
                if (target.empty()) continue;
                result.files.push_back({std::move(fullPath), std::move(target), e.fileSize});
                int totalFiles = static_cast<int>(result.files.size());
                if (progressCb && (totalFiles % 1000 == 0)) progressCb({false, 0, totalFiles, brls::getStr("other/installer/scanningFiles"), 0, 0});
            }
        }
    }

    result.dirs = utils::buildTargetDirs(rawDirs, tid);
    result.success = true;
    return result;
}

} // namespace

InstallResult installFromDir(const ModInfo& mod, const GameInfo& game,
    ModGameType modGameType, const std::vector<ModInfo>& allMods,
    std::function<void(const Progress&)> progressCb, std::stop_token token) {

    if (progressCb) progressCb({false, 0, 0, brls::getStr("other/installer/scanningFiles"), 0, 0});

    InstallResult result{};
    std::string tid = format::appIdHex(game.appId);
    std::string gameDirName = format::gameDirName(game.dirPath);

    // 扫描
    auto scan = scanDirMod(mod.path, tid, true, progressCb, token);

    if (!scan.success) {
        result.errorFile = scan.errorPath;
        result.errorMsg = scan.errorMsg;
        return result;
    }

    // 校验
    int totalFiles = static_cast<int>(scan.files.size());
    if (totalFiles == 0) {
        result.errorFile = mod.path;
        result.errorMsg = brls::getStr("other/installer/invalidModStructure");
        return result;
    }

    SpecialModInstallRules specialRules;
    if (!specialRules.init(modGameType, mod, game)) {
        result.errorFile = mod.path;
        result.errorMsg = brls::getStr("other/installer/specialRulesInitFailed");
        return result;
    }

    // 分配缓冲区
    utils::InstallBuf buf;
    if (!buf.alloc()) {
        result.errorMsg = brls::getStr("other/installer/memAllocFailed");
        return result;
    }

    // 引用计数
    ModFileRefCount refCount;
    refCount.load(game.dirPath + config::refCountFile);

    if (progressCb) progressCb({false, 0, totalFiles, brls::getStr("other/installer/detectingConflicts"), 0, 0});

    int copiedFiles = 0;
    for (int i = 0; i < totalFiles; ++i) {
        if (token.stop_requested()) return result;
        auto& file = scan.files[i];
        if (file.targetPath.empty()) continue;
        if (!specialRules.apply(file.targetPath)) {
            result.errorFile = file.targetPath;
            result.errorMsg = brls::getStr("other/installer/mhrisePatchNoLimit");
            return result;
        }

        int64_t diskCrc = crc::fromFile(file.targetPath.c_str(), buf.crc, crcBufSize, &token);
        if (diskCrc < 0) continue;

        int64_t srcCrc = crc::fromFile(file.sourcePath.c_str(), buf.crc, crcBufSize, &token);
        if (srcCrc < 0 || static_cast<uint32_t>(srcCrc) != static_cast<uint32_t>(diskCrc)) {
            if (progressCb) progressCb({false, copiedFiles, totalFiles, brls::getStr("other/installer/checkingConflicts"), 0, 0});
            result.errorFile = file.targetPath;
            result.conflictMod = utils::findConflictModName(file.targetPath, static_cast<uint32_t>(srcCrc), allMods, buf.crc, crcBufSize, &token);
            result.errorMsg = brls::getStr("other/installer/modConflict");
            return result;
        }

        file.skip = true;
        ++copiedFiles;
        refCount.increment(file.targetPath);
        if (progressCb) progressCb({false, copiedFiles, totalFiles, brls::getStr("other/installer/detectingConflicts"), 0, 0});
    }

    // 创建目录
    if (progressCb) progressCb({false, copiedFiles, totalFiles, brls::getStr("other/installer/buildingDirs"), 0, 0});

    for (auto& targetDir : scan.dirs) specialRules.applyDirectory(targetDir);

    utils::CreateDirsResult dirResult = utils::createDirs(scan.dirs);
    std::vector<std::string> writtenFiles;

    if (!dirResult.success) {
        utils::rollback(writtenFiles, scan.dirs, progressCb);
        result.errorFile = dirResult.errorPath;
        result.errorMsg = dirResult.errorMsg;
        return result;
    }

    bool cancelled = false;
    bool failed = false;

    // 小文件聚合缓存（直接使用 buf.io，零动态分配）
    struct CachedEntry {
        std::string targetPath;
        size_t offset;
        size_t size;
    };
    std::vector<CachedEntry> cache;
    size_t cacheSize = 0;

    // 将缓存中的小文件批量写盘
    auto flushCache = [&]() -> bool {
        for (auto& entry : cache) {
            if (token.stop_requested()) {
                cancelled = true;
                return false;
            }
            uint32_t rc = fs::writeFile(entry.targetPath, static_cast<char*>(buf.io) + entry.offset, entry.size);
            if (rc != 0) {
                writtenFiles.push_back(entry.targetPath);
                result.errorFile = entry.targetPath;
                result.errorMsg = brls::getStr("other/installer/writeFailed", format::resultHex(rc));
                failed = true;
                return false;
            }
            writtenFiles.push_back(entry.targetPath);
            ++copiedFiles;
            if (progressCb) progressCb({false, copiedFiles, totalFiles, utils::lastSegment(entry.targetPath), 0, 0});
        }
        cache.clear();
        cacheSize = 0;
        return true;
    };

    // Read + Write
    for (int i = 0; i < totalFiles; ++i) {
        if (token.stop_requested()) {
            cancelled = true;
            break;
        }

        const auto& file = scan.files[i];
        if (file.skip) continue;
        const char* fileName = utils::lastSegment(file.sourcePath);

        // pchtxt → 转换为 IPS 写入
        if (file.targetPath.empty()) {

            if (!cache.empty() && !flushCache()) break;

            if (progressCb) progressCb({false, copiedFiles + 1, totalFiles, fileName, 0, file.size});

            fs::FileReader reader;
            size_t bytesRead = 0;
            if (reader.open(file.sourcePath) != 0 || (bytesRead = reader.read(buf.io, ioBufSize)) == 0) {
                result.errorFile = file.sourcePath;
                result.errorMsg = brls::getStr("other/installer/readPchtxtFailed");
                failed = true;
                break;
            }

            auto pchtxt = utils::writePchtxt(buf.io, bytesRead, mod.dirName, gameDirName);
            if (!pchtxt.success) {
                if (!pchtxt.ipsPath.empty()) writtenFiles.push_back(pchtxt.ipsPath);
                result.errorFile = pchtxt.ipsDir.empty() ? file.sourcePath : pchtxt.ipsDir;
                result.errorMsg = pchtxt.errorMsg;
                failed = true;
                break;
            }
            scan.dirs.push_back(pchtxt.ipsDir);
            writtenFiles.push_back(pchtxt.ipsPath);
            ++copiedFiles;
            continue;
        }

        // 普通文件 — 小文件聚合缓存
        if (file.size <= smallFileThreshold) {
            if (cacheSize + static_cast<size_t>(file.size) > cacheMemLimit) {
                if (!flushCache()) break;
            }

            fs::FileReader reader;
            uint32_t openRc = reader.open(file.sourcePath, file.size);
            if (openRc != 0) {
                result.errorFile = file.sourcePath;
                result.errorMsg = brls::getStr("other/installer/readSourceFailed", format::resultHex(openRc));
                failed = true;
                break;
            }

            size_t bytesRead = reader.read(static_cast<char*>(buf.io) + cacheSize, static_cast<size_t>(file.size));
            if (bytesRead == 0 && file.size > 0) {
                result.errorFile = file.sourcePath;
                result.errorMsg = brls::getStr("other/installer/readSourceFailedNoCode");
                failed = true;
                break;
            }

            cache.push_back({file.targetPath, cacheSize, bytesRead});
            cacheSize += bytesRead;
            if (progressCb) progressCb({false, copiedFiles, totalFiles, brls::getStr("other/installer/aggregatingFiles"), static_cast<int64_t>(cacheSize), static_cast<int64_t>(cacheMemLimit)});
            continue;
        }

        // 普通文件 — 大文件：先清空缓存，再流式复制
        if (progressCb) progressCb({false, copiedFiles, totalFiles, fileName, 0, file.size});

        if (!cache.empty()) {
            if (!flushCache()) break;
        }

        fs::FileReader reader;
        uint32_t openRc = reader.open(file.sourcePath, file.size);
        if (openRc != 0) {
            result.errorFile = file.sourcePath;
            result.errorMsg = brls::getStr("other/installer/readSourceFailed", format::resultHex(openRc));
            failed = true;
            break;
        }

        fs::FileWriter writer;
        uint32_t rc = writer.open(file.targetPath, file.size);
        if (rc != 0) {
            result.errorFile = file.targetPath;
            result.errorMsg = brls::getStr("other/installer/createFileFailed", format::resultHex(rc));
            failed = true;
            break;
        }

        int64_t written = 0;
        bool writeError = false;
        size_t bytesRead;

        while ((bytesRead = reader.read(buf.io, ioBufSize)) > 0) {
            if (token.stop_requested()) {
                cancelled = true;
                break;
            }

            rc = writer.write(buf.io, bytesRead);
            if (rc != 0) {
                result.errorFile = file.targetPath;
                result.errorMsg = brls::getStr("other/installer/writeFailed", format::resultHex(rc));
                writeError = true;
                break;
            }

            written += static_cast<int64_t>(bytesRead);
            if (progressCb) progressCb({false, copiedFiles, totalFiles, fileName, written, file.size});
        }

        if (writeError || cancelled) {
            writtenFiles.push_back(file.targetPath);
            if (writeError) failed = true;
            break;
        }

        ++copiedFiles;
        writtenFiles.push_back(file.targetPath);
    }

    if (!failed && !cancelled) {
        flushCache();
    }

    // 失败/取消 → 回滚
    if (failed || cancelled) {
        utils::rollback(writtenFiles, scan.dirs, progressCb);
        return result;
    }

    refCount.save();
    specialRules.save();
    result.success = true;
    return result;
}

UninstallResult uninstallDir(const ModInfo& mod, const GameInfo& game,
    ModGameType modGameType, std::function<void(const Progress&)> progressCb) {

    if (progressCb) progressCb({false, 0, 0, brls::getStr("other/installer/scanningFiles"), 0, 0});

    UninstallResult result{};
    std::string tid = format::appIdHex(game.appId);
    std::string gameDirName = format::gameDirName(game.dirPath);
    fs::deleteFile(contentsPath + "/" + tid + "/romfs_metadata.bin");

    auto scan = scanDirMod(mod.path, tid, false, progressCb, {});

    if (!scan.success) {
        result.errorFile = scan.errorPath;
        result.errorMsg = scan.errorMsg;
        return result;
    }

    SpecialModUninstallRules specialRules;
    if (!specialRules.init(modGameType, mod, game, tid)) {
        result.errorFile = mod.path;
        result.errorMsg = brls::getStr("other/installer/specialUninstallRulesInitFailed");
        return result;
    }

    for (auto& targetDir : scan.dirs) specialRules.applyDirectory(targetDir);

    int totalFiles = static_cast<int>(scan.files.size());

    ModFileRefCount refCount;
    refCount.load(game.dirPath + config::refCountFile);

    for (int i = 0; i < totalFiles; ++i) {
        const auto& file = scan.files[i];
        const char* fileName = utils::lastSegment(file.sourcePath);

        if (file.targetPath.empty()) {
            if (progressCb) progressCb({false, i + 1, totalFiles, fileName, 0, 0});
            utils::removePchtxt(mod.dirName, gameDirName);
            continue;
        }

        if (progressCb) progressCb({false, i + 1, totalFiles, fileName, 0, 0});
        std::string targetPath = file.targetPath;
        if (!specialRules.apply(targetPath)) {
            result.errorFile = targetPath;
            result.errorMsg = brls::getStr("other/installer/specialUninstallRulesApplyFailed");
            return result;
        }
        if (refCount.decrement(targetPath)) fs::deleteFile(targetPath);
    }

    for (int i = static_cast<int>(scan.dirs.size()) - 1; i >= 0; --i) {
        fs::deleteEmptyDir(scan.dirs[i]);
    }

    refCount.save();
    if (!specialRules.save()) {
        result.errorMsg = brls::getStr("other/installer/mhriseReorderFailed");
        return result;
    }
    result.success = true;
    return result;
}

} // namespace ModInstaller
