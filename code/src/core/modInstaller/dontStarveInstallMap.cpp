/**
 * DontStarveInstallMap - 饥荒特殊 MOD 安装映射记录实现
 */

#include "core/modInstaller/dontStarveInstallMap.hpp"

bool DontStarveInstallMap::load(const std::string& path) {
    return m_json.load(path);
}

bool DontStarveInstallMap::save() {
    return m_json.save();
}

bool DontStarveInstallMap::hasMappings(const std::string& modDirName) {
    return m_json.hasRootKey(modDirName);
}

std::vector<std::string> DontStarveInstallMap::allTargetDirs() {
    std::vector<std::string> result;
    auto modDirNames = m_json.getRootKeys();
    for (const auto& modDirName : modDirNames) {
        auto sourceDirs = m_json.getKeys(modDirName);
        for (const auto& sourceDir : sourceDirs) {
            result.push_back(m_json.getString(modDirName, sourceDir));
        }
    }
    return result;
}

std::string DontStarveInstallMap::targetDir(const std::string& modDirName, const std::string& sourceDir) {
    return m_json.getString(modDirName, sourceDir);
}

void DontStarveInstallMap::setMapping(const std::string& modDirName, const std::string& sourceDir, const std::string& targetDir) {
    m_json.setString(modDirName, sourceDir, targetDir);
}

void DontStarveInstallMap::removeMod(const std::string& modDirName) {
    m_json.removeRootKey(modDirName);
}
