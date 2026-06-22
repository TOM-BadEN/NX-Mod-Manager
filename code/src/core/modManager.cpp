/**
 * ModManager - Mod 数据管理
 */

#include "core/modManager.hpp"
#include "core/modInstaller/utils.hpp"
#include "utils/fsHelper.hpp"
#include "utils/format.hpp"
#include "utils/strSort.hpp"
#include "api/mod.hpp"
#include "common/config.hpp"
#include <algorithm>
#include <chrono>


ModInstaller::utils::ModTidAndIpsDirs ModManager::collectAllTidAndIpsDirs() {
    ModInstaller::utils::ModTidAndIpsDirs result;
    std::string gameTid = format::appIdHex(m_game.appId);
    result.tidDirs.push_back(gameTid);

    for (const auto& mod : m_mods) {
        auto dirs = ModInstaller::utils::collectTidAndIpsDirs(mod, m_game);
        for (auto& tid : dirs.tidDirs) {
            if (std::find(result.tidDirs.begin(), result.tidDirs.end(), tid) == result.tidDirs.end()) {
                result.tidDirs.push_back(std::move(tid));
            }
        }
        for (auto& ips : dirs.ipsDirs) {
            if (std::find(result.ipsDirs.begin(), result.ipsDirs.end(), ips) == result.ipsDirs.end()) {
                result.ipsDirs.push_back(std::move(ips));
            }
        }
    }
    return result;
}

ModManager::ModManager(const GameInfo& game)
    : m_game(game), m_modGameType(modGameType::detect(game.appId))
{
    m_modJson.load(game.dirPath + config::modInfoFile);

    // 扫描 mod 子目录
    auto dirs = fs::listSubDirs(game.dirPath);

    for (const auto& dirName : dirs) {
        ModInfo info;

        // 显示名回滚：displayName → 目录名
        std::string displayName = m_modJson.getString(dirName, "displayName");
        info.displayName = displayName.empty() ? dirName : displayName;

        // 元数据
        info.type        = m_modJson.getString(dirName, "type", "other");
        info.description = m_modJson.getString(dirName, "description");
        info.modVersion  = m_modJson.getString(dirName, "modVersion");
        info.author      = m_modJson.getString(dirName, "author");
        info.authorLink  = m_modJson.getString(dirName, "authorLink");
        info.size        = m_modJson.getString(dirName, "size");
        info.isInstalled = m_modJson.getBool(dirName, "installed", false);
        info.modID       = std::stoi(m_modJson.getString(dirName, "modID", "-1"));
        info.fileCrc32   = m_modJson.getString(dirName, "fileCrc32");
        info.dirName     = dirName;
        info.gameVersion = m_modJson.getString(dirName, "gameVersion");

        std::string modDir = game.dirPath + "/" + dirName;
        info.isZip = !fs::listSubFiles(modDir, config::modFileExts).empty();
        info.path  = modDir;

        m_mods.push_back(std::move(info));
    }

    // 三级排序：已安装 > 未安装 → 类型分组 → 拼音
    strSort::sortAZ(m_mods, &ModInfo::displayName, &ModInfo::isInstalled, &ModInfo::type);
}

int ModManager::transitModCount() {
    return fs::countItems(config::transitDir, config::modFileExts);
}

std::vector<fs::DirEntry> ModManager::scanTransitMods() {
    return fs::listItems(config::transitDir, config::modFileExts);
}

int ModManager::addModsFormTransit(const std::string& dirPath, const std::vector<fs::DirEntry>& selectedMods) {
    int success = 0;
    for (const auto& mod : selectedMods) {
        std::string src = std::string(config::transitDir) + "/" + mod.name;
        if (mod.isFile) {
            // zip 文件：创建子目录（去掉扩展名），把 zip 移入
            auto dotPos = mod.name.rfind('.');
            std::string modDirName = (dotPos != std::string::npos) ? mod.name.substr(0, dotPos) : mod.name;
            std::string modDir = fs::ensureUniqueDirPath(dirPath + "/" + modDirName);
            fs::ensureDir(modDir);
            std::string dst = modDir + "/" + mod.name;
            if (fs::moveFile(src, dst)) success++;
        } else {
            // 目录 mod：检测冲突后移动
            std::string dst = fs::ensureUniqueDirPath(dirPath + "/" + mod.name);
            if (fs::moveDir(src, dst)) success++;
        }
    }
    return success;
}

int ModManager::addModsFormTransitForModList(const std::vector<fs::DirEntry>& selectedMods) {
    int success = 0;
    const std::string& dirPath = m_game.dirPath;

    for (const auto& mod : selectedMods) {
        std::string src = std::string(config::transitDir) + "/" + mod.name;
        std::string modDir;

        if (mod.isFile) {
            auto dotPos = mod.name.rfind('.');
            std::string baseName = (dotPos != std::string::npos) ? mod.name.substr(0, dotPos) : mod.name;
            modDir = fs::ensureUniqueDirPath(dirPath + "/" + baseName);
            fs::ensureDir(modDir);
            if (!fs::moveFile(src, modDir + "/" + mod.name)) continue;
        } else {
            modDir = fs::ensureUniqueDirPath(dirPath + "/" + mod.name);
            if (!fs::moveDir(src, modDir)) continue;
        }
        success++;

        std::string dirName = modDir.substr(modDir.rfind('/') + 1);
        ModInfo info;
        info.displayName = dirName;
        info.type        = "other";
        info.isZip       = !fs::listSubFiles(modDir, config::modFileExts).empty();
        info.modID       = -1;
        info.dirName     = dirName;
        info.path        = modDir;

        m_mods.push_back(std::move(info));
    }
    return success;
}

bool ModManager::hasInstalledMod(const std::string& tidPath) {
    JsonFile json;
    json.load(tidPath + config::modInfoFile);

    for (const auto& dirName : fs::listSubDirs(tidPath)) {
        if (json.getBool(dirName, "installed", false)) {
            return true;
        }
    }
    return false;
}

std::vector<ModInfo>& ModManager::mods() {
    return m_mods;
}

const GameInfo& ModManager::game() const {
    return m_game;
}

ModGameType ModManager::modGameType() const {
    return m_modGameType;
}

void ModManager::sort() {
    sort(m_sortAsc);
}

void ModManager::toggleSortAsc() {
    m_sortAsc = !m_sortAsc;
    sort(m_sortAsc);
}

bool ModManager::sortAsc() const { 
    return m_sortAsc; 
}

void ModManager::sort(bool ascending) {
    strSort::sortAZ(m_mods, &ModInfo::displayName, &ModInfo::isInstalled, &ModInfo::type, ascending);
}

void ModManager::setDisplayName(int index, const std::string& name) {
    m_mods[index].displayName = name;
    m_modJson.setString(m_mods[index].dirName, "displayName", name);
    m_modJson.save();
}

void ModManager::deleteCustomDisplayName(int index) {
    m_mods[index].displayName = m_mods[index].dirName;
    m_modJson.removeKey(m_mods[index].dirName, "displayName");
    m_modJson.save();
}

void ModManager::setType(int index, const std::string& type) {
    m_mods[index].type = type;
    m_modJson.setString(m_mods[index].dirName, "type", type);
    m_modJson.save();
}

void ModManager::setDescription(int index, const std::string& desc) {
    m_mods[index].description = desc;
    m_modJson.setString(m_mods[index].dirName, "description", desc);
    m_modJson.save();
}

void ModManager::setModVersion(int index, const std::string& version) {
    m_mods[index].modVersion = version;
    m_modJson.setString(m_mods[index].dirName, "modVersion", version);
    m_modJson.save();
}

void ModManager::setGameVersion(int index, const std::string& version) {
    m_mods[index].gameVersion = version;
    m_modJson.setString(m_mods[index].dirName, "gameVersion", version);
    m_modJson.save();
}

void ModManager::setAuthor(int index, const std::string& author) {
    m_mods[index].author = author;
    m_modJson.setString(m_mods[index].dirName, "author", author);
    m_modJson.save();
}

void ModManager::setSize(int index, const std::string& sizeStr) {
    m_mods[index].size = sizeStr;
    m_modJson.setString(m_mods[index].dirName, "size", sizeStr);
}

void ModManager::setFileCrc32(int index, const std::string& crc32) {
    m_mods[index].fileCrc32 = crc32;
    m_modJson.setString(m_mods[index].dirName, "fileCrc32", crc32);
}

std::string ModManager::buildUpdateCheckJson() const {
    std::string json = "{";
    bool first = true;
    for (const auto& mod : m_mods) {
        if (mod.modID <= 0 || mod.fileCrc32.empty()) continue;
        if (!first) json += ",";
        json += "\"" + std::to_string(mod.modID) + "\":\"" + mod.fileCrc32 + "\"";
        first = false;
    }
    json += "}";
    return first ? "" : json;
}

std::vector<int> ModManager::checkForUpdates(const std::string& gameTid, const std::string& modsJson, std::stop_token token) {
    auto result = api::mod::checkModUpdates(gameTid, modsJson, token);
    if (token.stop_requested()) return {};
    if (!result.success) return {};

    return result.updatedModIds;
}

void ModManager::saveJson() {
    m_modJson.save();
}

void ModManager::addModFromStore(ModInfo info) {
    m_modJson.setString(info.dirName, "displayName", info.displayName);
    m_modJson.setString(info.dirName, "type", info.type);
    m_modJson.setString(info.dirName, "description", info.description);
    m_modJson.setString(info.dirName, "modVersion", info.modVersion);
    m_modJson.setString(info.dirName, "gameVersion", info.gameVersion);
    m_modJson.setString(info.dirName, "author", info.author);
    m_modJson.setString(info.dirName, "authorLink", info.authorLink);
    m_modJson.setString(info.dirName, "size", info.size);
    m_modJson.setString(info.dirName, "modID", std::to_string(info.modID));
    m_modJson.setString(info.dirName, "fileCrc32", info.fileCrc32);
    m_modJson.save();

    m_mods.push_back(std::move(info));
    sort();
}

bool ModManager::updateModFromStore(int index, ModInfo info, const std::string& tempZipPath) {
    if (index < 0 || index >= static_cast<int>(m_mods.size())) return false;

    auto& old = m_mods[index];
    if (old.isInstalled) return false;

    const std::string dirName = old.dirName;
    const std::string modDir = old.path;
    auto zipFiles = fs::listSubFiles(modDir, config::modFileExts);
    std::string zipName = zipFiles.empty() ? dirName + ".zip" : zipFiles[0];
    std::string zipPath = modDir + "/" + zipName;

    if (fs::fileExists(zipPath)) fs::deleteFile(zipPath);
    if (!fs::moveFile(tempZipPath, zipPath)) return false;

    info.dirName = dirName;
    info.path = modDir;
    info.isInstalled = false;
    info.hasUpdate = false;
    info.isZip = true;

    m_modJson.setString(info.dirName, "displayName", info.displayName);
    m_modJson.setString(info.dirName, "type", info.type);
    m_modJson.setString(info.dirName, "description", info.description);
    m_modJson.setString(info.dirName, "modVersion", info.modVersion);
    m_modJson.setString(info.dirName, "gameVersion", info.gameVersion);
    m_modJson.setString(info.dirName, "author", info.author);
    m_modJson.setString(info.dirName, "authorLink", info.authorLink);
    m_modJson.setString(info.dirName, "size", info.size);
    m_modJson.setString(info.dirName, "modID", std::to_string(info.modID));
    m_modJson.setString(info.dirName, "fileCrc32", info.fileCrc32);
    m_modJson.setBool(info.dirName, "installed", false);
    m_modJson.save();

    m_mods[index] = std::move(info);
    sort();
    return true;
}

void ModManager::setPendingFocus(int modID) {
    m_pendingFocusModID = modID;
}

int ModManager::consumePendingFocus() {
    int id = m_pendingFocusModID;
    m_pendingFocusModID = -1;
    return id;
}

int ModManager::findByModID(int modID) const {
    for (int i = 0; i < static_cast<int>(m_mods.size()); i++) {
        if (m_mods[i].modID == modID) return i;
    }
    return -1;
}

void ModManager::removeModFromModList(int idx) {
    auto& mod = m_mods[idx];

    fs::ensureDir(config::transitDir);
    std::string dest = fs::ensureUniqueDirPath(std::string(config::transitDir) + mod.dirName);
    fs::moveDir(mod.path, dest);

    m_modJson.removeRootKey(mod.dirName);
    m_modJson.save();

    m_mods.erase(m_mods.begin() + idx);
}

int ModManager::findByDirName(const std::string& dirName) const {
    for (int i = 0; i < static_cast<int>(m_mods.size()); i++) {
        if (m_mods[i].dirName == dirName) return i;
    }
    return -1;
}

ModInstaller::InstallResult ModManager::installMod(int index,
    std::function<void(const ModInstaller::Progress&)> progressCb, std::stop_token token) {

    auto result = ModInstaller::install(m_mods[index], m_game, m_modGameType, m_mods, progressCb, token);
    if (result.success) {
        m_mods[index].isInstalled = true;
        m_modJson.setBool(m_mods[index].dirName, "installed", true);
        m_modJson.save();
    }
    return result;
}

ModInstaller::UninstallResult ModManager::uninstallMod(int index,
    std::function<void(const ModInstaller::Progress&)> progressCb) {

    auto result = ModInstaller::uninstall(m_mods[index], m_game, m_modGameType, progressCb);
    if (result.success) {
        m_mods[index].isInstalled = false;
        m_modJson.setBool(m_mods[index].dirName, "installed", false);
        m_modJson.save();
    }
    return result;
}

bool ModManager::disableMods() {
    std::string gameDirName = format::gameDirName(m_game.dirPath);
    auto allDirs = collectAllTidAndIpsDirs();

    // rename contents/{tid} → contents/{tid}-disable
    for (const auto& tidDir : allDirs.tidDirs) {
        std::string tidPath = ModInstaller::contentsPath + "/" + tidDir;
        std::string tidDisable = tidPath + "-disable";
        if (fs::dirExists(tidPath)) {
            if (!fs::moveDir(tidPath, tidDisable)) return false;
        }
    }

    // rename exefs_patches 下 .ips → .ips-disable
    for (const auto& ipsDirName : allDirs.ipsDirs) {
        std::string ipsDir = ModInstaller::atmospherePath + "/exefs_patches/" + ipsDirName;
        if (!fs::dirExists(ipsDir)) continue;
        auto files = fs::listSubFiles(ipsDir, {".ips"});
        for (const auto& fname : files) {
            fs::moveFile(ipsDir + "/" + fname, ipsDir + "/" + fname + "-disable");
        }
    }

    // pchtxt 生成的 ips 目录
    for (const auto& mod : m_mods) {
        std::string ipsDir = ModInstaller::atmospherePath + "/exefs_patches/" + mod.dirName + "_" + gameDirName;
        if (!fs::dirExists(ipsDir)) continue;
        auto files = fs::listSubFiles(ipsDir, {".ips"});
        for (const auto& fname : files) {
            fs::moveFile(ipsDir + "/" + fname, ipsDir + "/" + fname + "-disable");
        }
    }

    return true;
}

bool ModManager::enableMods() {
    std::string gameDirName = format::gameDirName(m_game.dirPath);
    auto allDirs = collectAllTidAndIpsDirs();

    // rename contents/{tid}-disable → contents/{tid}
    for (const auto& tidDir : allDirs.tidDirs) {
        std::string tidPath = ModInstaller::contentsPath + "/" + tidDir;
        std::string tidDisable = tidPath + "-disable";
        if (fs::dirExists(tidDisable)) {
            if (!fs::moveDir(tidDisable, tidPath)) return false;
        }
    }

    // rename exefs_patches 下 .ips-disable → .ips
    for (const auto& ipsDirName : allDirs.ipsDirs) {
        std::string ipsDir = ModInstaller::atmospherePath + "/exefs_patches/" + ipsDirName;
        if (!fs::dirExists(ipsDir)) continue;
        auto files = fs::listSubFiles(ipsDir, {".ips-disable"});
        for (const auto& fname : files) {
            std::string restored = fname.substr(0, fname.size() - 8); // 去掉 "-disable"
            fs::moveFile(ipsDir + "/" + fname, ipsDir + "/" + restored);
        }
    }

    // pchtxt 生成的 ips 目录
    for (const auto& mod : m_mods) {
        std::string ipsDir = ModInstaller::atmospherePath + "/exefs_patches/" + mod.dirName + "_" + gameDirName;
        if (!fs::dirExists(ipsDir)) continue;
        auto files = fs::listSubFiles(ipsDir, {".ips-disable"});
        for (const auto& fname : files) {
            std::string restored = fname.substr(0, fname.size() - 8); // 去掉 "-disable"
            fs::moveFile(ipsDir + "/" + fname, ipsDir + "/" + restored);
        }
    }

    return true;
}

fs::RemoveResult ModManager::forceClean(
    std::stop_token token,
    std::function<void(int deleted, int total, const char* fileName)> onProgress)
{
    using Clock = std::chrono::steady_clock;
    auto startTime = Clock::now();

    std::string gameDirName = format::gameDirName(m_game.dirPath);
    auto allDirs = collectAllTidAndIpsDirs();

    // 删除所有 TID 对应的 contents 目录（含 -disable）
    fs::RemoveResult result{};
    result.status = fs::RemoveResult::Completed;

    for (const auto& tidDir : allDirs.tidDirs) {
        std::string tidPath = ModInstaller::contentsPath + "/" + tidDir;
        std::string tidDisable = tidPath + "-disable";

        if (fs::dirExists(tidPath)) {
            result = fs::removeDirContentsWithProgress(tidPath, token, onProgress);
            if (result.status != fs::RemoveResult::Completed) return result;
            fs::removeDirAll(tidPath);
        }

        if (fs::dirExists(tidDisable)) {
            result = fs::removeDirContentsWithProgress(tidDisable, token, onProgress);
            if (result.status != fs::RemoveResult::Completed) return result;
            fs::removeDirAll(tidDisable);
        }
    }

    // 删除 exefs_patches 目录
    for (const auto& ipsDirName : allDirs.ipsDirs) {
        std::string ipsDir = ModInstaller::atmospherePath + "/exefs_patches/" + ipsDirName;
        if (fs::dirExists(ipsDir) && !fs::removeDirAll(ipsDir)) {
            result.status = fs::RemoveResult::FsError;
            result.errorPath = ipsDir;
            return result;
        }
    }

    // 删除 pchtxt 生成的 ips 目录
    for (const auto& mod : m_mods) {
        std::string ipsDir = ModInstaller::atmospherePath + "/exefs_patches/" + mod.dirName + "_" + gameDirName;
        if (fs::dirExists(ipsDir) && !fs::removeDirAll(ipsDir)) {
            result.status = fs::RemoveResult::FsError;
            result.errorPath = ipsDir;
            return result;
        }
    }

    // 删除引用计数文件
    std::string refCountPath = m_game.dirPath + config::refCountFile;
    if (fs::fileExists(refCountPath) && !fs::deleteFile(refCountPath)) {
        result.status = fs::RemoveResult::FsError;
        result.errorPath = refCountPath;
        return result;
    }

    // 删除怪猎特殊 pak 安装映射记录
    std::string mhriseMapPath = m_game.dirPath + config::mhriseInstallMapFile;
    if (fs::fileExists(mhriseMapPath) && !fs::deleteFile(mhriseMapPath)) {
        result.status = fs::RemoveResult::FsError;
        result.errorPath = mhriseMapPath;
        return result;
    }

    // 重置所有 mod 的 installed 标记
    for (auto& mod : m_mods) {
        mod.isInstalled = false;
        m_modJson.setBool(mod.dirName, "installed", false);
    }
    m_modJson.save();

    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - startTime).count();
    result.elapsed = format::elapsed(elapsedMs);
    return result;
}
