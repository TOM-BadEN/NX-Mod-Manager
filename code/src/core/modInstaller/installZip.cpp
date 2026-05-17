/**
 * ModInstaller - ZIP 模组安装与卸载实现
 */

#include "core/modInstaller/installZip.hpp"
#include "core/modInstaller/utils.hpp"
#include "core/modFileRefCount.hpp"
#include "common/config.hpp"
#include "utils/fsHelper.hpp"
#include "utils/zipReader.hpp"
#include "utils/crc32.hpp"
#include "utils/format.hpp"
#include <borealis/core/i18n.hpp>

#include <algorithm>

namespace ModInstaller {

namespace {

struct ZipModFile {
    const ZipEntry* entry;
    std::string targetPath;   // pchtxt 为空，安装时由转换结果决定
    bool skip = false;        // CRC 匹配后标记跳过，不需复制
};

std::vector<ZipModFile> buildModFileList(const std::vector<ZipEntry>& files, const std::string& tid) {
    std::vector<ZipModFile> modFiles;
    for (const auto& entry : files) {
        if (utils::endsWith(entry.path, pchtxtExt)) {
            modFiles.push_back({&entry, {}});
        } else {
            std::string target = utils::buildTargetPath(entry.path, tid);
            if (target.empty()) continue;
            modFiles.push_back({&entry, std::move(target)});
        }
    }
    return modFiles;
}

} // namespace

InstallResult installFromZip(const ModInfo& mod, const GameInfo& game,
    const std::vector<ModInfo>& allMods, std::function<void(const Progress&)> progressCb,
    std::stop_token token) {

    if (progressCb) progressCb({false, 0, 0, brls::getStr("other/installer/scanningFiles"), 0, 0});

    InstallResult result{};

    std::string zipPath = utils::getZipModFilePath(mod.path);
    if (zipPath.empty()) {
        result.errorFile = mod.path;
        result.errorMsg = brls::getStr("other/installer/zipNotFound");
        return result;
    }

    ZipReader zip(zipPath);
    if (!zip.isOpen()) {
        result.errorFile = zipPath;
        result.errorMsg = brls::getStr("other/installer/zipOpenFailed");
        return result;
    }

    std::string tid = format::appIdHex(game.appId);
    std::string gameDirName = format::gameDirName(game.dirPath);
    
    auto modFiles = buildModFileList(zip.files(), tid);
    int totalFiles = static_cast<int>(modFiles.size());
    if (totalFiles == 0) {
        result.errorFile = zipPath;
        result.errorMsg = brls::getStr("other/installer/invalidModStructure");
        return result;
    }

    if (progressCb) progressCb({false, 0, totalFiles, brls::getStr("other/installer/scanningFiles"), 0, 0});

    // 分配缓冲区
    utils::InstallBuf buf;
    if (!buf.alloc()) {
        result.errorMsg = brls::getStr("other/installer/memAllocFailed");
        return result;
    }

    ModFileRefCount refCount;
    refCount.load(game.dirPath + config::refCountFile);

    if (progressCb) progressCb({false, 0, totalFiles, brls::getStr("other/installer/detectingConflicts"), 0, 0});

    int copiedFiles = 0;

    // CRC 冲突检测（独立阶段，避免与解压写入交替刷 FS 缓存）
    for (int i = 0; i < totalFiles; ++i) {
        if (token.stop_requested()) return result;
        if (modFiles[i].targetPath.empty()) continue;

        int64_t diskCrc = crc::fromFile(modFiles[i].targetPath.c_str(), buf.crc, crcBufSize);
        if (diskCrc < 0) continue;

        if (static_cast<uint32_t>(diskCrc) != modFiles[i].entry->crc32) {
            if (progressCb) progressCb({false, copiedFiles, totalFiles, brls::getStr("other/installer/checkingConflicts"), 0, 0});
            result.errorFile = modFiles[i].targetPath;
            result.conflictMod = utils::findConflictModName(modFiles[i].targetPath, modFiles[i].entry->crc32, allMods, buf.crc, crcBufSize);
            result.errorMsg = brls::getStr("other/installer/modConflict");
            return result;
        }

        modFiles[i].skip = true;
        ++copiedFiles;
        refCount.increment(modFiles[i].targetPath);
        if (progressCb) progressCb({false, copiedFiles, totalFiles, brls::getStr("other/installer/detectingConflicts"), 0, 0});
    }

    // 创建目录
    if (progressCb) progressCb({false, copiedFiles, totalFiles, brls::getStr("other/installer/buildingDirs"), 0, 0});

    auto targetDirs = utils::buildTargetDirs(zip.dirs(), tid);
    std::vector<std::string> writtenFiles;

    utils::CreateDirsResult dirResult = utils::createDirs(targetDirs);
    if (!dirResult.success) {
        utils::rollback(writtenFiles, targetDirs, progressCb);
        result.errorFile = dirResult.errorPath;
        result.errorMsg = dirResult.errorMsg;
        return result;
    }

    bool cancelled = false;
    bool failed = false;

    // 解压 + 写入
    for (int i = 0; i < totalFiles; ++i) {
        if (token.stop_requested()) {
            cancelled = true;
            break;
        }
        if (modFiles[i].skip) continue;

        const char* fileName = utils::lastSegment(modFiles[i].entry->path);

        // pchtxt → 转换为 IPS 写入
        if (modFiles[i].targetPath.empty()) {
            
            if (progressCb) progressCb({false, copiedFiles + 1, totalFiles, fileName, 0, modFiles[i].entry->uncompressedSize});

            size_t pchtxtSize = zip.readFile(*modFiles[i].entry, buf.io, ioBufSize);
            if (pchtxtSize == 0) {
                result.errorFile = modFiles[i].entry->path;
                result.errorMsg = brls::getStr("other/installer/readPchtxtFailed");
                failed = true;
                break;
            }

            auto pchtxt = utils::writePchtxt(buf.io, pchtxtSize, mod.dirName, gameDirName);
            if (!pchtxt.success) {
                if (!pchtxt.ipsPath.empty()) writtenFiles.push_back(pchtxt.ipsPath);
                result.errorFile = pchtxt.ipsDir.empty() ? modFiles[i].entry->path : pchtxt.ipsDir;
                result.errorMsg = pchtxt.errorMsg;
                failed = true;
                break;
            }
            targetDirs.push_back(pchtxt.ipsDir);
            writtenFiles.push_back(pchtxt.ipsPath);
            ++copiedFiles;
            continue;
        }

        // 普通文件
        if (progressCb) progressCb({false, copiedFiles, totalFiles, fileName, 0, modFiles[i].entry->uncompressedSize});

        // 小文件：一次性解压 + 写入
        if (modFiles[i].entry->uncompressedSize <= static_cast<int64_t>(ioBufSize)) {
            size_t bytesRead = zip.readFile(*modFiles[i].entry, buf.io, ioBufSize);

            if (bytesRead == 0 && modFiles[i].entry->uncompressedSize > 0) {
                result.errorFile = modFiles[i].entry->path;
                result.errorMsg = brls::getStr("other/installer/zipExtractFailed");
                failed = true;
                break;
            }

            uint32_t rc = fs::writeFile(modFiles[i].targetPath, buf.io, bytesRead);
            if (rc != 0) {
                writtenFiles.push_back(modFiles[i].targetPath);
                result.errorFile = modFiles[i].targetPath;
                result.errorMsg = brls::getStr("other/installer/writeFailed", format::resultHex(rc));
                failed = true;
                break;
            }

            writtenFiles.push_back(modFiles[i].targetPath);
            ++copiedFiles;
            continue;
        }

        if (!zip.beginRead(*modFiles[i].entry)) {
            result.errorFile = modFiles[i].entry->path;
            result.errorMsg = brls::getStr("other/installer/zipReadFailed");
            failed = true;
            break;
        }

        // 大文件：流式分块读写
        fs::FileWriter writer;
        uint32_t rc = writer.open(modFiles[i].targetPath, modFiles[i].entry->uncompressedSize);
        if (rc != 0) {
            zip.endRead();
            result.errorFile = modFiles[i].targetPath;
            result.errorMsg = brls::getStr("other/installer/createFileFailed", format::resultHex(rc));
            failed = true;
            break;
        }

        int64_t written = 0;
        bool writeError = false;
        size_t bytesRead;

        while ((bytesRead = zip.read(buf.io, ioBufSize)) > 0) {
            if (token.stop_requested()) {
                cancelled = true;
                break;
            }

            rc = writer.write(buf.io, bytesRead);
            if (rc != 0) {
                result.errorFile = modFiles[i].targetPath;
                result.errorMsg = brls::getStr("other/installer/writeFailed", format::resultHex(rc));
                writeError = true;
                break;
            }

            written += static_cast<int64_t>(bytesRead);
            if (progressCb) progressCb({false, copiedFiles, totalFiles, fileName, written, modFiles[i].entry->uncompressedSize});
        }

        zip.endRead();

        if (writeError || cancelled) {
            writtenFiles.push_back(modFiles[i].targetPath);
            if (writeError) failed = true;
            break;
        }

        writtenFiles.push_back(modFiles[i].targetPath);
        ++copiedFiles;
    }

    // 失败或取消 → 回滚
    if (failed || cancelled) {
        utils::rollback(writtenFiles, targetDirs, progressCb);
        return result;
    }

    refCount.save();
    result.success = true;
    return result;
}

UninstallResult uninstallZip(const ModInfo& mod, const GameInfo& game, std::function<void(const Progress&)> progressCb) {

    if (progressCb) progressCb({false, 0, 0, brls::getStr("other/installer/scanningFiles"), 0, 0});

    UninstallResult result{};

    std::string zipPath = utils::getZipModFilePath(mod.path);
    if (zipPath.empty()) {
        result.errorFile = mod.path;
        result.errorMsg = brls::getStr("other/installer/zipNotFound");
        return result;
    }

    ZipReader zip(zipPath);
    if (!zip.isOpen()) {
        result.errorFile = zipPath;
        result.errorMsg = brls::getStr("other/installer/zipOpenFailed");
        return result;
    }

    const auto& files = zip.files();
    if (files.empty()) {
        result.errorFile = zipPath;
        result.errorMsg = brls::getStr("other/installer/zipNoFiles");
        return result;
    }
    
    int totalFiles = static_cast<int>(files.size());
    if (progressCb) progressCb({false, 0, totalFiles, brls::getStr("other/installer/scanningDirs"), 0, 0});

    std::string tid = format::appIdHex(game.appId);
    std::string gameDirName = format::gameDirName(game.dirPath);
    auto targetDirs = utils::buildTargetDirs(zip.dirs(), tid);

    ModFileRefCount refCount;
    refCount.load(game.dirPath + config::refCountFile);

    // 遍历 files，逐个删除
    for (int i = 0; i < totalFiles; ++i) {
        const auto& entry = files[i];
        const char* fileName = utils::lastSegment(entry.path);

        if (utils::endsWith(entry.path, pchtxtExt)) {
            if (progressCb) progressCb({false, i + 1, totalFiles, fileName, 0, 0});
            utils::removePchtxt(mod.dirName, gameDirName);
            continue;
        }

        std::string targetPath = utils::buildTargetPath(entry.path, tid);
        if (targetPath.empty()) continue;

        if (progressCb) progressCb({false, i + 1, totalFiles, fileName, 0, 0});

        if (refCount.decrement(targetPath)) fs::deleteFile(targetPath);
    }

    // 遍历 dirs，倒序清理空目录
    for (int i = static_cast<int>(targetDirs.size()) - 1; i >= 0; --i) {
        fs::deleteEmptyDir(targetDirs[i]);
    }

    refCount.save();
    result.success = true;
    return result;
}

} // namespace ModInstaller
