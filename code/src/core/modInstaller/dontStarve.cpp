/**
 * dontStarve - 饥荒特殊安装规则实现
 */

#include "core/modInstaller/dontStarve.hpp"

#include "common/config.hpp"
#include "core/modInstaller/utils.hpp"
#include "utils/fsHelper.hpp"

#include <fmt/core.h>

namespace {
    constexpr const char* modsPath = "romfs/mods/";

    struct ModDirPath {
        size_t dirPos;
        size_t dirLength;
        std::string dirName;
    };

    /**
     * @brief 解析通用安装器选中的 romfs/mods 直接下级目录
     * @param path 源路径或目标路径
     * @param result 解析出的目录信息
     */
    bool getModDirPath(const std::string& path, ModDirPath& result, bool isDirectory) {
        size_t modsPos = ModInstaller::utils::findKeywordPos(path);
        if (modsPos == std::string::npos) return false;
        if (path.compare(modsPos, std::char_traits<char>::length(modsPath), modsPath) != 0) return false;

        size_t dirPos = modsPos + std::char_traits<char>::length(modsPath);
        if (dirPos >= path.size()) return false;

        size_t dirEnd = path.find('/', dirPos);
        if (!isDirectory && dirEnd == std::string::npos) return false;

        size_t dirLength = (dirEnd == std::string::npos ? path.size() : dirEnd) - dirPos;
        if (dirLength == 0) return false;

        result.dirPos = dirPos;
        result.dirLength = dirLength;
        result.dirName = path.substr(dirPos, dirLength);
        return true;
    }

    /**
     * @brief 生成指定编号的 BM 目录名
     */
    std::string makeBmDirName(int number) {
        return fmt::format("BM{:04}", number);
    }

    int bmNumberFromDirName(const std::string& dirName) {
        return std::stoi(dirName.substr(2));
    }

    /**
     * @brief 判断饥荒实际 BM 目标路径是否已经存在
     */
    bool targetPathExists(const std::string& targetPath, const ModDirPath& modPath, const std::string& targetDir) {
        std::string bmPath = targetPath.substr(0, modPath.dirPos) + targetDir;
        return fs::dirExists(bmPath);
    }

}

namespace ModInstaller::dontStarve {

InstallPrecheckResult checkBeforeInstall(const std::string& tid) {
    std::string frameworkPath = contentsPath + "/" + tid + "/romfs/scripts/modindex.lua";
    return fs::fileExists(frameworkPath) ? InstallPrecheckResult::Ok : InstallPrecheckResult::FrameworkMissing;
}

bool DontStarveInstallPlanner::prepare(const std::string& gameDirPath, const std::string& modDirName) {
    m_installMap.load(gameDirPath + config::dontStarveInstallMapFile);

    m_usedBmNumbers.reset();
    auto targetDirs = m_installMap.allTargetDirs();
    for (const auto& targetDir : targetDirs) {
        int number = bmNumberFromDirName(targetDir);
        m_usedBmNumbers.set(number);
    }

    m_modDirName = modDirName;
    m_nextBmNumber = 1;
    while (m_nextBmNumber <= 9999 && m_usedBmNumbers.test(m_nextBmNumber)) ++m_nextBmNumber;
    m_changed = false;
    return true;
}

void DontStarveInstallPlanner::processFilePath(std::string& targetPath) {
    processTargetPath(targetPath, false);
}

void DontStarveInstallPlanner::processDirectoryPath(std::string& targetPath) {
    processTargetPath(targetPath, true);
}

void DontStarveInstallPlanner::processTargetPath(std::string& targetPath, bool isDirectory) {
    ModDirPath modPath;
    if (!getModDirPath(targetPath, modPath, isDirectory)) return;

    std::string targetDir = m_installMap.targetDir(m_modDirName, modPath.dirName);
    if (targetDir.empty()) {
        while (m_nextBmNumber <= 9999) {
            if (m_usedBmNumbers.test(m_nextBmNumber)) {
                ++m_nextBmNumber;
                continue;
            }

            targetDir = makeBmDirName(m_nextBmNumber);
            m_usedBmNumbers.set(m_nextBmNumber);
            ++m_nextBmNumber;

            if (targetPathExists(targetPath, modPath, targetDir)) {
                targetDir.clear();
                continue;
            }

            m_installMap.setMapping(m_modDirName, modPath.dirName, targetDir);
            m_changed = true;
            break;
        }
    }

    if (targetDir.empty()) return;

    targetPath.replace(modPath.dirPos, modPath.dirLength, targetDir);
}

bool DontStarveInstallPlanner::save() {
    if (!m_changed) return true;

    m_installMap.save();
    return true;
}

bool DontStarveUninstallPlanner::prepare(const std::string& gameDirPath, const std::string& modDirName) {
    m_installMap.load(gameDirPath + config::dontStarveInstallMapFile);

    m_modDirName = modDirName;
    m_hasMappings = m_installMap.hasMappings(modDirName);
    return true;
}

bool DontStarveUninstallPlanner::processFilePath(std::string& targetPath) {
    return processTargetPath(targetPath, false);
}

void DontStarveUninstallPlanner::processDirectoryPath(std::string& targetPath) {
    processTargetPath(targetPath, true);
}

bool DontStarveUninstallPlanner::processTargetPath(std::string& targetPath, bool isDirectory) {
    ModDirPath modPath;
    if (!getModDirPath(targetPath, modPath, isDirectory)) return true;

    std::string targetDir = m_installMap.targetDir(m_modDirName, modPath.dirName);
    if (targetDir.empty()) return true;

    targetPath.replace(modPath.dirPos, modPath.dirLength, targetDir);
    return true;
}

bool DontStarveUninstallPlanner::save() {
    if (!m_hasMappings) return true;

    m_installMap.removeMod(m_modDirName);
    m_installMap.save();
    return true;
}

} // namespace ModInstaller::dontStarve
