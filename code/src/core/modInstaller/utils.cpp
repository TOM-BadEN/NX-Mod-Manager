/**
 * ModInstaller - 共享工具函数实现
 */

#include "core/modInstaller/utils.hpp"
#include "common/config.hpp"
#include "utils/fsHelper.hpp"
#include "utils/zipReader.hpp"
#include "utils/crc32.hpp"
#include "utils/pchtxtConverter.hpp"
#include "utils/format.hpp"
#include <algorithm>
#include <borealis/core/i18n.hpp>

namespace {
// 获取 mod 源目录内的原始目录与文件列表（相对路径）。目录安装和 ZIP 安装
// 最终转换成同一种 RawModPaths，后续策略不需要知道数据来自哪种载体。
struct RawModPaths {
    std::vector<std::string> dirs;
    std::vector<std::string> files;
};

RawModPaths collectRawPaths(const ModInfo& mod) {
    RawModPaths result;
    if (mod.isZip) {
        // ZIP 文件列表不能只扫描目录，否则 NRO IPS 无法确认目录中存在合法 .ips 文件。
        std::string zipPath = ModInstaller::utils::getZipModFilePath(mod.path);
        if (zipPath.empty()) return result;
        ZipReader zip(zipPath);
        if (!zip.isOpen()) return result;
        result.dirs = zip.dirs();
        for (const auto& entry : zip.files()) result.files.push_back(entry.path);
        return result;
    }

    std::vector<std::string> stack;
    stack.push_back(mod.path);
    size_t baseLen = mod.path.size();

    while (!stack.empty()) {
        // 使用显式栈遍历普通目录，避免递归深度受模组目录层级影响。
        std::string cur = std::move(stack.back());
        stack.pop_back();

        fs::DirReader reader;
        if (reader.open(cur) != 0) continue;

        std::vector<fs::DirEntry> batch;
        while (true) {
            if (reader.read(batch) != 0) break;
            if (batch.empty()) break;
            for (auto& e : batch) {
                std::string full = cur + "/" + e.name;
                if (e.isFile) {
                    result.files.push_back(full.substr(baseLen + 1));
                    continue;
                }
                result.dirs.push_back(full.substr(baseLen + 1));
                stack.push_back(std::move(full));
            }
        }
    }
    return result;
}
} // namespace

namespace ModInstaller::utils {

// ============================================================================
// 工具函数
// ============================================================================

const char* lastSegment(const std::string& path) {
    size_t pos = path.rfind('/');
    return (pos == std::string::npos) ? path.c_str() : path.c_str() + pos + 1;
}

bool endsWith(const std::string& str, const std::string& suffix) {
    return str.size() >= suffix.size() && str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool hasDotPathSegment(const std::string& path) {
    if (!path.empty() && path[0] == '.') return true;

    for (size_t i = 0; i + 1 < path.size(); ++i) {
        if (path[i] == '/' && path[i + 1] == '.') return true;
    }

    return false;
}

size_t findKeywordPos(const std::string& path) {
    // 保留原 utils API，实际识别逻辑集中到策略注册表，避免各扫描流程维护不同关键词列表。
    return targets::findKeywordPos(path);
}

std::vector<std::string> buildTargetDirs(const std::vector<std::string>& dirs, const std::string& tid, bool skipDotEntries) {
    std::vector<std::string> filtered;
    filtered.reserve(dirs.size());
    for (const auto& dir : dirs) {
        if (!skipDotEntries || !hasDotPathSegment(dir)) filtered.push_back(dir);
    }
    // 隐藏目录过滤仍由旧 API 控制，路径映射和 NRO IPS 层级校验交给策略。
    return targets::buildTargetDirs(filtered, tid);
}

std::string buildTargetPath(const std::string& path, const std::string& tid) {
    return targets::buildTargetPath(path, tid);
}

CreateDirsResult createDirs(const std::vector<std::string>& dirs) {
    CreateDirsResult result;
    for (const auto& dir : dirs) {
        uint32_t rc = fs::createDir(dir);
        if (rc != 0) {
            result.success = false;
            result.errorPath = dir;
            result.errorMsg = brls::getStr("other/installer/createDirFailed", format::resultHex(rc));
            break;
        }
    }
    return result;
}

std::string getZipModFilePath(const std::string& modDir) {
    auto zipFiles = fs::listSubFiles(modDir, config::modFileExts);
    if (zipFiles.empty()) return {};
    return modDir + "/" + zipFiles[0];
}

void rollback(const std::vector<std::string>& createdFiles, const std::vector<std::string>& createdDirs, std::function<void(const Progress&)>& progressCb) {

    int total = static_cast<int>(createdFiles.size() + createdDirs.size());
    int seq = 0;

    for (const auto& file : createdFiles) {
        if (progressCb) progressCb({true, ++seq, total, lastSegment(file), 0, 0});
        fs::deleteFile(file);
    }

    for (int i = static_cast<int>(createdDirs.size()) - 1; i >= 0; --i) {
        if (progressCb) progressCb({true, ++seq, total, lastSegment(createdDirs[i]), 0, 0});
        fs::deleteEmptyDir(createdDirs[i]);
    }
}

PchtxtWriteResult writePchtxt(const void* data, size_t len, const std::string& modDirName, const std::string& gameDirName) {
    PchtxtWriteResult r;

    auto ips = PchtxtConverter::convert(data, len);
    if (!ips.success) {
        r.errorMsg = brls::getStr("other/installer/pchtxtConvertFailed", ips.errorMsg);
        return r;
    }

    r.ipsDir = atmospherePath + "/exefs_patches/" + modDirName + "_" + gameDirName;
    if (!fs::ensureDir(r.ipsDir)) {
        r.errorMsg = brls::getStr("other/installer/pchtxtCreateDirFailed");
        return r;
    }

    r.ipsPath = r.ipsDir + "/" + ips.nsobid + ".ips";
    uint32_t rc = fs::writeFile(r.ipsPath, ips.ipsData.data(), ips.ipsData.size());
    if (rc != 0) {
        r.errorMsg = brls::getStr("other/installer/writeIpsFailed", format::resultHex(rc));
        return r;
    }

    r.success = true;
    return r;
}

void removePchtxt(const std::string& modDirName, const std::string& gameDirName) {
    std::string ipsDir = atmospherePath + "/exefs_patches/" + modDirName + "_" + gameDirName;
    fs::removeDirAll(ipsDir);
}

std::string findConflictModName(const std::string& targetPath, uint32_t conflictCrc, const std::vector<ModInfo>& allMods, void* crcBuf, size_t crcBufLen, std::stop_token* token) {

    size_t kwPos = findKeywordPos(targetPath);
    if (kwPos == std::string::npos) return {};

    std::string relFile = targetPath.substr(kwPos);
    bool isRomfsBin = (relFile == "romfs.bin");

    std::string relDir, fileName;
    if (!isRomfsBin) {
        size_t lastSlash = relFile.rfind('/');
        relDir = relFile.substr(0, lastSlash);
        fileName = relFile.substr(lastSlash + 1);
    }

    for (const auto& mod : allMods) {
        if (token && token->stop_requested()) return {};
        if (!mod.isInstalled) continue;

        if (mod.isZip) {
            std::string zipPath = getZipModFilePath(mod.path);
            if (zipPath.empty()) continue;
            ZipReader zip(zipPath);
            if (!zip.isOpen()) continue;

            for (const auto& entry : zip.files()) {
                size_t pos = findKeywordPos(entry.path);
                if (pos == std::string::npos) continue;
                if (entry.path.substr(pos) == relFile && entry.crc32 != conflictCrc) return mod.displayName;
            }
            continue;
        }

        std::vector<std::string> dirStack;
        dirStack.push_back(mod.path);

        while (!dirStack.empty()) {
            if (token && token->stop_requested()) return {};
            std::string curPath = std::move(dirStack.back());
            dirStack.pop_back();

            fs::DirReader reader;
            if (reader.open(curPath) != 0) continue;

            if (isRomfsBin) {
                std::string candidate = curPath + "/romfs.bin";
                int64_t srcCrc = crc::fromFile(candidate.c_str(), crcBuf, crcBufLen, token);
                if (srcCrc >= 0 && static_cast<uint32_t>(srcCrc) != conflictCrc) {
                    return mod.displayName;
                }
            }

            std::vector<fs::DirEntry> batch;
            while (true) {
                if (reader.read(batch) != 0) break;
                if (batch.empty()) break;

                for (auto& e : batch) {
                    if (e.isFile) continue;
                    std::string fullPath = curPath + "/" + e.name;

                    if (!isRomfsBin) {
                        std::string rel = fullPath.substr(mod.path.size() + 1);
                        size_t pos = findKeywordPos(rel);
                        if (pos != std::string::npos && rel.substr(pos) == relDir) {
                            std::string sourceFile = fullPath + "/" + fileName;
                            int64_t srcCrc = crc::fromFile(sourceFile.c_str(), crcBuf, crcBufLen, token);
                            if (srcCrc >= 0 && static_cast<uint32_t>(srcCrc) != conflictCrc) {
                                return mod.displayName;
                            }
                        }
                    }

                    dirStack.push_back(std::move(fullPath));
                }
            }
        }
    }

    return brls::getStr("other/installer/unknownMod");
}

std::vector<targets::ManagedTarget> collectManagedTargets(const ModInfo& mod, const GameInfo& game) {
    // 先完整收集目录和文件，再统一调用策略层；同一模组的 contents 与 nro_patches
    // 会因此返回两个目标，并由上层作为一个模组管理。
    RawModPaths sourcePaths = collectRawPaths(mod);
    return targets::collectManagedTargets(sourcePaths.dirs, sourcePaths.files, format::appIdHex(game.appId));
}

} // namespace ModInstaller::utils
