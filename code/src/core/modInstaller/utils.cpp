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
// 获取 mod 源目录内的原始目录列表（相对路径）
std::vector<std::string> collectRawDirs(const ModInfo& mod) {
    if (mod.isZip) {
        std::string zipPath = ModInstaller::utils::getZipModFilePath(mod.path);
        if (zipPath.empty()) return {};
        ZipReader zip(zipPath);
        if (!zip.isOpen()) return {};
        return zip.dirs();
    }

    std::vector<std::string> rawDirs;
    std::vector<std::string> stack;
    stack.push_back(mod.path);
    size_t baseLen = mod.path.size();

    while (!stack.empty()) {
        std::string cur = std::move(stack.back());
        stack.pop_back();

        fs::DirReader reader;
        if (reader.open(cur) != 0) continue;

        std::vector<fs::DirEntry> batch;
        while (true) {
            if (reader.read(batch) != 0) break;
            if (batch.empty()) break;
            for (auto& e : batch) {
                if (e.isFile) continue;
                std::string full = cur + "/" + e.name;
                rawDirs.push_back(full.substr(baseLen + 1));
                stack.push_back(std::move(full));
            }
        }
    }
    return rawDirs;
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

size_t findKeywordPos(const std::string& path) {
    for (const auto& kw : modKeywords) {
        size_t pos = 0;
        while ((pos = path.find(kw, pos)) != std::string::npos) {
            bool leftOk = (pos == 0 || path[pos - 1] == '/');
            size_t end = pos + kw.size();
            bool rightOk = (end == path.size() || path[end] == '/');
            if (leftOk && rightOk) return pos;
            pos = end;
        }
    }
    return std::string::npos;
}

// 从关键词前一级目录提取 TID（16位hex），未找到返回空
std::string extractTidBeforeKeyword(const std::string& path, size_t keywordPos) {
    if (keywordPos < 17) return {};
    if (path[keywordPos - 1] != '/') return {};
    if (keywordPos >= 18 && path[keywordPos - 18] != '/') return {};
    for (size_t i = keywordPos - 17; i < keywordPos - 1; ++i) {
        char c = path[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return {};
    }
    return path.substr(keywordPos - 17, 16);
}

std::vector<std::string> buildTargetDirs(const std::vector<std::string>& dirs, const std::string& tid) {
    std::vector<std::string> result;
    result.reserve(dirs.size() + 2);

    bool hasExefsPatches = false;
    std::vector<std::string> addedTids;

    for (const auto& dir : dirs) {
        // 跳过不含关键词的目录（如 contents/、纯 TID 目录等）
        size_t pos = findKeywordPos(dir);
        if (pos == std::string::npos) continue;

        // 截取关键词及之后的相对路径，如 "romfs/data" "exefs_patches/xxx"
        const std::string& rel = dir.substr(pos);

        // exefs_patches → 直接映射到 /atmosphere/exefs_patches/...
        if (rel.compare(0, 13, "exefs_patches") == 0) {
            if (!hasExefsPatches) {
                result.push_back(atmospherePath + "/exefs_patches");
                hasExefsPatches = true;
            }
            result.push_back(atmospherePath + "/" + rel);
            continue;
        }

        // 尝试从关键词前一级目录提取 TID，提取失败则使用游戏 TID
        std::string modTid = extractTidBeforeKeyword(dir, pos);
        const std::string& targetTid = modTid.empty() ? tid : modTid;

        // 确保 contents/<tid> 目录只添加一次
        if (std::find(addedTids.begin(), addedTids.end(), targetTid) == addedTids.end()) {
            result.push_back(contentsPath + "/" + targetTid);
            addedTids.push_back(targetTid);
        }

        // 添加实际目标目录，如 /atmosphere/contents/<tid>/romfs/data
        result.push_back(contentsPath + "/" + targetTid + "/" + rel);
    }

    return result;
}

std::string buildTargetPath(const std::string& path, const std::string& tid) {
    size_t pos = findKeywordPos(path);
    if (pos == std::string::npos) return {};
    std::string rel = path.substr(pos);
    if (rel.compare(0, 13, "exefs_patches") == 0) return atmospherePath + "/" + rel;
    std::string modTid = extractTidBeforeKeyword(path, pos);
    return contentsPath + "/" + (modTid.empty() ? tid : modTid) + "/" + rel;
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

void rollback(const std::vector<std::string>& createdFiles, const std::vector<std::string>& createdDirs,
    std::function<void(const Progress&)>& progressCb) {

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

std::string findConflictModName(const std::string& targetPath, uint32_t conflictCrc,
    const std::vector<ModInfo>& allMods, void* crcBuf, size_t crcBufLen, std::stop_token* token) {

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

ModTidAndIpsDirs collectTidAndIpsDirs(const ModInfo& mod, const GameInfo& game) {
    ModTidAndIpsDirs result;
    std::string tid = format::appIdHex(game.appId);

    for (const auto& dir : collectRawDirs(mod)) {
        size_t pos = findKeywordPos(dir);
        if (pos == std::string::npos) continue;
        std::string rel = dir.substr(pos);

        if (rel.compare(0, 13, "exefs_patches") == 0) {
            if (rel.size() <= 14) continue;
            std::string name = rel.substr(14);
            if (std::find(result.ipsDirs.begin(), result.ipsDirs.end(), name) == result.ipsDirs.end()) result.ipsDirs.push_back(std::move(name));
        } else {
            std::string modTid = extractTidBeforeKeyword(dir, pos);
            const std::string& target = modTid.empty() ? tid : modTid;
            if (std::find(result.tidDirs.begin(), result.tidDirs.end(), target) == result.tidDirs.end()) result.tidDirs.push_back(target);
        }
    }

    return result;
}

} // namespace ModInstaller::utils
