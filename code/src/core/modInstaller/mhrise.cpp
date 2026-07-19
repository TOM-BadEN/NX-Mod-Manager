/**
 * mhrise - 怪物猎人 崛起特殊安装规则实现
 */

#include "core/modInstaller/mhrise.hpp"

#include "common/config.hpp"
#include "core/modInstaller/mhriseInstallMap.hpp"
#include "core/modInstaller/utils.hpp"
#include "utils/format.hpp"
#include "utils/fsHelper.hpp"

#include <algorithm>
#include <cstring>
#include <fmt/core.h>
#include <vector>

namespace {
    const char* pakPrefix = "re_chunk_000.pak.patch_";
    const char* pakSuffix = ".pak";

    struct SpecialPakPath {
        size_t filePos;
        std::string fileName;
    };

    /**
     * @brief 判断文件名是否符合怪猎特殊 pak 的前后缀规则
     * @param fileName 文件名
     */
    bool hasPakPrefixAndSuffix(const std::string& fileName) {
        size_t prefixLen = std::strlen(pakPrefix);
        size_t suffixLen = std::strlen(pakSuffix);

        if (fileName.size() <= prefixLen + suffixLen) return false;
        if (fileName.compare(0, prefixLen, pakPrefix) != 0) return false;
        if (fileName.compare(fileName.size() - suffixLen, suffixLen, pakSuffix) != 0) return false;

        return true;
    }

    /**
     * @brief 从标准三位编号 pak 文件名中读取 patch 编号
     * @param fileName 文件名
     */
    int patchNoFromPakFileName(const std::string& fileName) {
        size_t prefixLen = std::strlen(pakPrefix);
        size_t suffixLen = std::strlen(pakSuffix);

        if (fileName.size() != prefixLen + 3 + suffixLen) return -1;
        if (!hasPakPrefixAndSuffix(fileName)) return -1;

        int patchNo = 0;
        for (size_t i = prefixLen; i < prefixLen + 3; ++i) {
            char c = fileName[i];
            if (c < '0' || c > '9') return -1;
            patchNo = patchNo * 10 + (c - '0');
        }
        return patchNo;
    }

    /**
     * @brief 解析 romfs 根目录下的怪猎特殊 pak 路径
     * @param targetPath 标准流程生成的目标路径
     * @param result 解析出的文件位置和文件名
     */
    bool getSpecialPakPath(const std::string& targetPath, SpecialPakPath& result) {
        size_t filePos = targetPath.rfind('/');
        if (filePos == std::string::npos) return false;

        std::string fileName = targetPath.substr(filePos + 1);
        if (!hasPakPrefixAndSuffix(fileName)) return false;

        std::string parentDir = targetPath.substr(0, filePos);
        size_t parentPos = parentDir.rfind('/');
        std::string parentName = parentPos == std::string::npos ? parentDir : parentDir.substr(parentPos + 1);
        if (parentName != "romfs") return false;

        result.filePos = filePos;
        result.fileName = std::move(fileName);
        return true;
    }

    /**
     * @brief 根据游戏版本获取起始 patch 编号，找不到返回 0
     * @param version 游戏版本
     */
    int basePatchNoForVersion(const std::string& version) {
        std::string clean = format::cleanVersion(version);

        if (clean == "v3.4.1") return 12;
        if (clean == "v3.5.0") return 13;
        if (clean == "v3.6.1") return 14;
        if (clean == "v3.7.0") return 16;
        if (clean == "v3.8.0") return 17;
        if (clean == "v3.9.0") return 18;
        if (clean == "v3.9.1") return 19;
        if (clean == "v10.0.2") return 4;
        if (clean == "v10.0.3") return 5;
        if (clean == "v11.0.1") return 7;
        if (clean == "v11.0.2") return 8;
        if (clean == "v12.0.0") return 9;
        if (clean == "v12.0.1") return 10;
        if (clean == "v13.0.0") return 11;
        if (clean == "v14.0.0") return 12;
        if (clean == "v15.0.0") return 13;
        if (clean == "v16.0.0") return 15;
        if (clean == "v16.0.1") return 16;
        if (clean == "v16.0.2") return 17;

        return 0;
    }

    /**
     * @brief 根据 patch 编号生成怪猎 pak 文件名
     * @param patchNo patch 编号
     */
    std::string makePakFileName(int patchNo) {
        return fmt::format("re_chunk_000.pak.patch_{:03}.pak", patchNo);
    }

}

namespace ModInstaller::mhrise {

InstallPrecheckResult checkBeforeInstall(const std::string& currentVersion, bool gameInstalled, const std::string& gameDirPath) {

    if (!gameInstalled) return InstallPrecheckResult::GameNotInstalled;
    if (currentVersion.empty() || currentVersion == "...") return InstallPrecheckResult::VersionUnsupported;

    std::string cleanVersion = format::cleanVersion(currentVersion);
    if (basePatchNoForVersion(cleanVersion) == 0) return InstallPrecheckResult::VersionUnsupported;

    MHRiseInstallMap installMap;
    installMap.load(gameDirPath + config::mhriseInstallMapFile);
    if (installMap.hasRecords() && installMap.gameVersion() != cleanVersion) return InstallPrecheckResult::VersionChanged;

    return InstallPrecheckResult::Ok;
}

bool MHRiseInstallPlanner::prepare(const std::string& gameDirPath, const std::string& currentVersion, const std::string& modDirName) {
    m_installMap.load(gameDirPath + config::mhriseInstallMapFile);

    m_modDirName = modDirName;
    m_gameVersion = format::cleanVersion(currentVersion);
    m_lastPatchNo = m_installMap.lastPatchNo();
    m_patchNo = m_lastPatchNo == 0 ? basePatchNoForVersion(m_gameVersion) : m_lastPatchNo + 1;
    m_ready = m_patchNo > 0;
    m_changed = false;

    return m_ready;
}

bool MHRiseInstallPlanner::processTargetPath(std::string& targetPath) {
    SpecialPakPath pakPath;
    if (!getSpecialPakPath(targetPath, pakPath)) return true;

    if (!m_ready || m_patchNo > 99) return false;

    std::string targetPak = makePakFileName(m_patchNo);
    targetPath = targetPath.substr(0, pakPath.filePos + 1) + targetPak;
    m_installMap.setMapping(m_modDirName, pakPath.fileName, targetPak);
    m_lastPatchNo = m_patchNo;
    ++m_patchNo;
    m_changed = true;

    return true;
}

bool MHRiseInstallPlanner::save() {
    if (!m_changed) return true;

    m_installMap.setGameVersion(m_gameVersion);
    m_installMap.setLastPatchNo(m_lastPatchNo);
    m_installMap.increaseInstalledCount();
    return m_installMap.save();
}

bool MHRiseUninstallPlanner::prepare(const std::string& gameDirPath, const std::string& tid, const std::string& modDirName) {
    m_installMap.load(gameDirPath + config::mhriseInstallMapFile);

    m_modDirName = modDirName;
    m_romfsPath = contentsPath + "/" + tid + "/romfs";
    m_minRemovedPatchNo = 0;
    m_removedCount = 0;
    m_changed = false;

    return true;
}

bool MHRiseUninstallPlanner::processTargetPath(std::string& targetPath) {
    SpecialPakPath pakPath;
    if (!getSpecialPakPath(targetPath, pakPath)) return true;

    std::string targetPak = m_installMap.targetPak(m_modDirName, pakPath.fileName);
    if (targetPak.empty()) return true;

    targetPath = targetPath.substr(0, pakPath.filePos + 1) + targetPak;
    int patchNo = patchNoFromPakFileName(targetPak);
    if (m_removedCount == 0 || patchNo < m_minRemovedPatchNo) m_minRemovedPatchNo = patchNo;
    ++m_removedCount;
    m_changed = true;
    return true;
}

bool MHRiseUninstallPlanner::save() {
    if (!m_changed) return true;

    struct MovePlan {
        MHRisePakMapping mapping;
        int oldPatchNo;
        std::string newPak;
    };

    auto allMappings = m_installMap.allMappings();
    std::vector<MovePlan> movePlans;
    int lastPatchNo = 0;

    for (const auto& mapping : allMappings) {
        if (mapping.modDirName == m_modDirName) continue;

        int oldPatchNo = patchNoFromPakFileName(mapping.targetPak);
        if (oldPatchNo <= m_minRemovedPatchNo) {
            if (oldPatchNo > lastPatchNo) lastPatchNo = oldPatchNo;
            continue;
        }

        int newPatchNo = oldPatchNo - m_removedCount;
        if (newPatchNo > lastPatchNo) lastPatchNo = newPatchNo;

        std::string newPak = makePakFileName(newPatchNo);
        movePlans.push_back({mapping, oldPatchNo, newPak});
    }

    std::sort(movePlans.begin(), movePlans.end(), [](const MovePlan& a, const MovePlan& b) {
        return a.oldPatchNo < b.oldPatchNo;
    });

    for (const auto& plan : movePlans) {
        std::string oldPath = m_romfsPath + "/" + plan.mapping.targetPak;
        std::string newPath = m_romfsPath + "/" + plan.newPak;
        if (!fs::moveFile(oldPath, newPath)) return false;
    }

    for (const auto& plan : movePlans) {
        m_installMap.setMapping(plan.mapping.modDirName, plan.mapping.sourcePak, plan.newPak);
    }

    m_installMap.removeMod(m_modDirName);
    m_installMap.decreaseInstalledCount();
    m_installMap.setLastPatchNo(lastPatchNo);
    return m_installMap.save();
}

} // namespace ModInstaller::mhrise
