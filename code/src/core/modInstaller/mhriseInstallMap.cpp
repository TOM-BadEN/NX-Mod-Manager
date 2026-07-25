/**
 * MHRiseInstallMap - 怪猎特殊 pak 安装映射记录实现
 */

#include "core/modInstaller/mhriseInstallMap.hpp"

namespace {
    constexpr const char* metaKey = "__meta";
}

bool MHRiseInstallMap::load(const std::string& path) {
    return m_json.load(path);
}

bool MHRiseInstallMap::save() {
    return m_json.save();
}

std::string MHRiseInstallMap::gameVersion() {
    return m_json.getString(metaKey, "gameVersion");
}

bool MHRiseInstallMap::hasRecords() {
    return m_json.getInt(metaKey, "installedCount", 0) > 0;
}

void MHRiseInstallMap::increaseInstalledCount() {
    int count = m_json.getInt(metaKey, "installedCount", 0) + 1;
    m_json.setInt(metaKey, "installedCount", count);
}

void MHRiseInstallMap::decreaseInstalledCount() {
    int count = m_json.getInt(metaKey, "installedCount", 0) - 1;
    if (count < 0) count = 0;
    m_json.setInt(metaKey, "installedCount", count);
}

int MHRiseInstallMap::lastPatchNo() {
    return m_json.getInt(metaKey, "lastPatchNo", 0);
}

void MHRiseInstallMap::setLastPatchNo(int patchNo) {
    m_json.setInt(metaKey, "lastPatchNo", patchNo);
}

void MHRiseInstallMap::setGameVersion(const std::string& version) {
    m_json.setString(metaKey, "gameVersion", version);
}

std::vector<MHRisePakMapping> MHRiseInstallMap::mappings(const std::string& modDirName) {
    std::vector<MHRisePakMapping> result;
    auto sourcePaks = m_json.getKeys(modDirName);
    for (const auto& sourcePak : sourcePaks) {
        std::string targetPak = m_json.getString(modDirName, sourcePak);
        if (targetPak.empty()) continue;
        result.push_back({modDirName, sourcePak, targetPak});
    }
    return result;
}

std::vector<MHRisePakMapping> MHRiseInstallMap::allMappings() {
    std::vector<MHRisePakMapping> result;
    auto modDirNames = m_json.getRootKeys();
    for (const auto& modDirName : modDirNames) {
        if (modDirName == metaKey) continue;

        auto modMappings = mappings(modDirName);
        result.insert(result.end(), modMappings.begin(), modMappings.end());
    }
    return result;
}

std::string MHRiseInstallMap::targetPak(const std::string& modDirName, const std::string& sourcePak) {
    return m_json.getString(modDirName, sourcePak);
}

void MHRiseInstallMap::setMapping(const std::string& modDirName, const std::string& sourcePak, const std::string& targetPak) {
    m_json.setString(modDirName, sourcePak, targetPak);
}

void MHRiseInstallMap::removeMapping(const std::string& modDirName, const std::string& sourcePak) {
    m_json.removeKey(modDirName, sourcePak);
    if (m_json.getKeys(modDirName).empty()) m_json.removeRootKey(modDirName);
}

void MHRiseInstallMap::removeMod(const std::string& modDirName) {
    m_json.removeRootKey(modDirName);
}
