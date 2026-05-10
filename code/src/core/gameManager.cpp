/**
 * GameManager - 游戏数据管理
 */

#include "core/gameManager.hpp"
#include "core/downloadRecord.hpp"
#include "common/config.hpp"
#include "utils/format.hpp"
#include "utils/textClean.hpp"
#include "utils/strSort.hpp"
#include <cstdlib>
#include <unordered_map>

GameManager::GameManager() {
    
    m_jsonCache.load(config::gameInfoPath);

    // 确保 mods 目录存在
    fs::ensureDir(config::modsRoot);

    // 扫描 /mods2/ 获取所有游戏目录名
    auto dirs = fs::listSubDirs(config::modsRoot);
    std::unordered_map<uint64_t, int> seenAppIds;

    for (const auto& dirName : dirs) {
        std::string dirPath = std::string(config::modsRoot) + dirName + "/";

        // 找 hex 子目录 → appId
        auto subDirs = fs::listSubDirs(dirPath);
        uint64_t appId = 0;
        std::string appIdHex;

        for (const auto& sub : subDirs) {
            uint64_t val = format::appIdFromHex(sub);
            if (val != 0 && format::appIdIsValid(val)) {
                appId = val;
                appIdHex = sub;
                break;
            }
        }

        if (appId == 0) continue;

        // 数 mod（只数子目录，每个子目录 = 一个 mod）
        std::string appIdPath = dirPath + appIdHex + "/";
        int modCount = fs::countDirs(appIdPath);
        if (modCount == 0) continue;

        // 用 appId 查 JSON
        std::string appIdKey = format::appIdHex(appId);

        std::string displayName = m_jsonCache.getString(appIdKey, "displayName");
        std::string gameName = m_jsonCache.getString(appIdKey, "gameName");
        std::string version = m_jsonCache.getString(appIdKey, "version");
        bool isFavorite = m_jsonCache.getBool(appIdKey, "favorite", false);
        bool isModsDisabled = m_jsonCache.getBool(appIdKey, "modsDisabled", false);
        bool hasInstalledMod = m_jsonCache.getBool(appIdKey, "hasInstalledMod", false);

        // 显示名回滚链：displayName → gameName → 目录名
        std::string finalName;
        if (!displayName.empty()) finalName = displayName;
        else if (!gameName.empty()) finalName = gameName;
        else finalName = dirName;

        // 填充 GameInfo
        GameInfo info;
        info.displayName = finalName;
        info.version = version.empty() ? "..." : version;
        info.modCount = std::to_string(modCount);
        info.appId = appId;
        info.dirPath = dirPath + appIdHex;
        info.isFavorite = isFavorite;
        info.isModsDisabled = isModsDisabled;
        info.hasInstalledMod = hasInstalledMod;

        // 检测重复 appId
        auto it = seenAppIds.find(appId);
        if (it != seenAppIds.end()) {
            if (!m_games[it->second].isDuplicate) {
                m_duplicateCount++;
                m_games[it->second].isDuplicate = true;
            }
            info.isDuplicate = true;
        } else {
            seenAppIds[appId] = static_cast<int>(m_games.size());
        }

        m_games.push_back(std::move(info));
    }

    // 首次预排序（支持拼音）
    strSort::sortAZ(m_games, &GameInfo::displayName, &GameInfo::isFavorite);
}

std::vector<GameInfo>& GameManager::games() {
    return m_games;
}

std::string GameManager::getAppIdKey(int idx) {
    return format::appIdHex(m_games[idx].appId);
}

std::string GameManager::getDirName(int idx) {
    return format::gameDirName(m_games[idx].dirPath);
}

int GameManager::duplicateCount() const {
    return m_duplicateCount;
}

void GameManager::sort(bool ascending) {
    strSort::sortAZ(m_games, &GameInfo::displayName, &GameInfo::isFavorite, ascending);
}

int GameManager::findByAppId(uint64_t appId) {
    for (int i = 0; i < static_cast<int>(m_games.size()); i++) {
        if (m_games[i].appId == appId) return i;
    }
    return -1;
}

int GameManager::findInstalledByAppId(uint64_t appId) {
    for (int i = 0; i < static_cast<int>(m_installedGames.size()); i++) {
        if (m_installedGames[i].appId == appId) return i;
    }
    return -1;
}

void GameManager::openNacp() {
    if (!m_nacp) m_nacp = new GameNACP();
}

void GameManager::closeNacp() {
    delete m_nacp;
    m_nacp = nullptr;
}

GameMetadata GameManager::fetchMetadata(int idx) {
    if (!m_nacp) return {};
    return m_nacp->getGameNACP(m_games[idx].appId);
}

api::game::NameResult GameManager::fetchDisplayName(int idx, std::stop_token token) {
    std::string tid = format::appIdHex(m_games[idx].appId);
    return api::game::fetchName(tid, token);
}

void GameManager::setFavorite(int idx, bool favorite) {
    auto& game = m_games[idx];
    game.isFavorite = favorite;
    std::string appIdKey = format::appIdHex(game.appId);
    m_jsonCache.setBool(appIdKey, "favorite", favorite);
    m_jsonCache.save();
}

void GameManager::setModsDisabled(int idx, bool disabled) {
    auto& game = m_games[idx];
    game.isModsDisabled = disabled;
    std::string appIdKey = format::appIdHex(game.appId);
    m_jsonCache.setBool(appIdKey, "modsDisabled", disabled);
    m_jsonCache.save();
}

void GameManager::setHasInstalledMod(int idx, bool value) {
    auto& game = m_games[idx];
    game.hasInstalledMod = value;
    std::string appIdKey = format::appIdHex(game.appId);
    if (value) m_jsonCache.setBool(appIdKey, "hasInstalledMod", true);
    else m_jsonCache.removeKey(appIdKey, "hasInstalledMod");
    m_jsonCache.save();
}

void GameManager::setVersion(int idx, const std::string& version, bool save) {
    auto& game = m_games[idx];
    game.version = version;
    std::string appIdKey = format::appIdHex(game.appId);
    m_jsonCache.setString(appIdKey, "version", version);
    if (save) m_jsonCache.save();
}

void GameManager::setGameName(int idx, const std::string& name, bool save) {
    auto& game = m_games[idx];
    std::string appIdKey = format::appIdHex(game.appId);
    m_jsonCache.setString(appIdKey, "gameName", name);
    if (m_jsonCache.getString(appIdKey, "displayName").empty()) game.displayName = name;
    if (save) m_jsonCache.save();
}

void GameManager::setDisplayName(int idx, const std::string& name) {
    auto& game = m_games[idx];
    game.displayName = name;
    std::string appIdKey = format::appIdHex(game.appId);
    m_jsonCache.setString(appIdKey, "displayName", name);
    m_jsonCache.save();
}

std::string GameManager::getRestoredDisplayName(int idx) {
    auto& game = m_games[idx];
    std::string appIdKey = format::appIdHex(game.appId);

    // 回滚链：gameName → 目录名
    std::string restored = m_jsonCache.getString(appIdKey, "gameName");
    if (restored.empty()) {
        restored = getDirName(idx);
    }
    return restored;
}

void GameManager::deleteCustomDisplayName(int idx, const std::string& restoredName) {
    auto& game = m_games[idx];
    game.displayName = restoredName;
    std::string appIdKey = format::appIdHex(game.appId);
    m_jsonCache.removeKey(appIdKey, "displayName");
    m_jsonCache.save();
}

void GameManager::removeGame(int idx) {
    auto& game = m_games[idx];

    // 确保中转站存在
    fs::ensureDir(config::transitDir);

    // 清理商店下载记录
    auto modDirs = fs::listSubDirs(game.dirPath);
    JsonFile modJson;
    modJson.load(game.dirPath + config::modInfoFile);
    DownloadRecord record;
    record.load();
    for (const auto& dirName : modDirs) {
        std::string modIdStr = modJson.getString(dirName, "modID");
        if (!modIdStr.empty()) record.remove(std::atoi(modIdStr.c_str()));
    }
    record.save();

    // 移动所有 MOD 到中转站
    for (const auto& dirName : modDirs) {
        std::string src  = game.dirPath + "/" + dirName;
        std::string dest = fs::ensureUniqueDirPath(std::string(config::transitDir) + dirName);
        fs::moveDir(src, dest);
    }

    // 删除游戏目录（game.dirPath 的父目录）
    auto pos = game.dirPath.rfind('/');
    std::string gameDir = game.dirPath.substr(0, pos);
    fs::removeDirAll(gameDir);

    // 清理 JSON
    std::string appIdKey = format::appIdHex(game.appId);
    m_jsonCache.removeRootKey(appIdKey);
    m_jsonCache.save();

    // 同步到已安装列表，供添加页面显示
    int installedIdx = findInstalledByAppId(game.appId);
    if (installedIdx >= 0) m_installedGames[installedIdx].modCount = "";

    // 从列表中移除
    m_games.erase(m_games.begin() + idx);
}

std::string GameManager::addGame(size_t installedIdx, int modCount) {
    auto& installed = m_installedGames[installedIdx];
    uint64_t appId = installed.appId;

    int existing = findByAppId(appId);
    if (existing >= 0) {
        int old = std::stoi(m_games[existing].modCount);
        std::string newCount = std::to_string(old + modCount);
        m_games[existing].modCount = newCount;
        installed.modCount = newCount;  // 同步到已安装列表，供添加页面显示
        return m_games[existing].dirPath;
    }

    std::string gameName = installed.displayName;
    std::string version = installed.version;

    std::string englishName = GameNACP::getEnglishName(appId);
    std::string dirName = textClean::safeDirName(englishName);
    if (dirName.empty()) dirName = format::appIdHex(appId);

    std::string appIdHex = format::appIdHex(appId);
    std::string parentPath = fs::ensureUniqueDirPath(std::string(config::modsRoot) + dirName);
    std::string dirPath = parentPath + "/" + appIdHex;
    fs::ensureDir(dirPath);

    m_jsonCache.setString(appIdHex, "gameName", gameName);
    m_jsonCache.setString(appIdHex, "version", version);
    m_jsonCache.save();

    GameInfo info;
    info.displayName = gameName;
    info.version = version;
    info.modCount = std::to_string(modCount);
    info.iconId = installed.iconId;
    info.appId = appId;
    info.dirPath = dirPath;
    m_games.push_back(info);
    installed.modCount = info.modCount;  // 同步到已安装列表，供添加页面显示

    return dirPath;
}

std::string GameManager::addGameFromStore(const std::string& gameTid, const std::string& gameNameEn, const std::string& gameName) {
    uint64_t appId = format::appIdFromHex(gameTid);

    int existing = findByAppId(appId);
    if (existing >= 0) {
        int old = std::stoi(m_games[existing].modCount);
        m_games[existing].modCount = std::to_string(old + 1);
        return m_games[existing].dirPath;
    }

    std::string dirName = textClean::safeDirName(gameNameEn);
    if (dirName.empty()) dirName = gameTid;

    std::string parentPath = fs::ensureUniqueDirPath(std::string(config::modsRoot) + dirName);
    std::string dirPath = parentPath + "/" + gameTid;
    fs::ensureDir(dirPath);

    m_jsonCache.setString(gameTid, "displayName", gameName);
    m_jsonCache.save();

    GameInfo info;
    info.displayName = gameName;
    info.version = "...";
    info.modCount = "1";
    info.appId = appId;
    info.dirPath = dirPath;
    m_games.push_back(info);

    return dirPath;
}

std::string GameManager::addGameByTid(const std::string& tid, int modCount) {
    uint64_t appId = format::appIdFromHex(tid);

    int existing = findByAppId(appId);
    if (existing >= 0) {
        int old = std::stoi(m_games[existing].modCount);
        m_games[existing].modCount = std::to_string(old + modCount);
        return m_games[existing].dirPath;
    }

    std::string parentPath = fs::ensureUniqueDirPath(std::string(config::modsRoot) + tid);
    std::string dirPath = parentPath + "/" + tid;
    fs::ensureDir(dirPath);

    m_jsonCache.setString(tid, "displayName", tid);
    m_jsonCache.save();

    GameInfo info;
    info.displayName = tid;
    info.version = "...";
    info.modCount = std::to_string(modCount);
    info.appId = appId;
    info.dirPath = dirPath;
    m_games.push_back(info);

    return dirPath;
}

fs::RemoveResult GameManager::clearTransit(
    std::stop_token token,
    std::function<void(int, int, const char*)> onProgress)
{
    return fs::removeDirContentsWithProgress(config::transitDir, token, onProgress);
}

void GameManager::resetState(std::function<void(int, int, const std::string&)> onProgress) {
    int total = static_cast<int>(m_games.size());

    for (int i = 0; i < total; i++) {
        auto& game = m_games[i];
        if (onProgress) onProgress(i + 1, total, game.displayName);

        // 清理 modInfo.json: 删 installed、modID
        JsonFile modJson;
        if (modJson.load(game.dirPath + config::modInfoFile)) {
            auto modDirs = fs::listSubDirs(game.dirPath);
            for (const auto& dir : modDirs) {
                modJson.removeKey(dir, "installed");
                modJson.removeKey(dir, "modID");
            }
            modJson.save();
        }

        // 删除 modFileRefCount.json
        fs::deleteFile(game.dirPath + config::refCountFile);

        // 清理 gameInfo.json 中的状态 key
        std::string key = format::appIdHex(game.appId);
        m_jsonCache.removeKey(key, "hasInstalledMod");
        m_jsonCache.removeKey(key, "modsDisabled");

        // 同步内存
        game.hasInstalledMod = false;
        game.isModsDisabled = false;
    }
    m_jsonCache.save();

    // 清理商店下载记录
    fs::deleteFile(config::downloadRecordPath);
}

void GameManager::saveJsonCache() {
    m_jsonCache.save();
}

GameMetadata GameManager::fetchInstalledMetadata(size_t idx) {
    if (!m_nacp || idx >= m_installedGames.size()) return {};
    return m_nacp->getGameNACP(m_installedGames[idx].appId);
}

void GameManager::sortInstalledGames(bool ascending) {
    if (m_installedGames.empty()) return;
    // 弹出虚拟游戏（appId==0 的头部条目），排序真实游戏，再插回头部
    InstalledGameInfo vg = m_installedGames.front();
    m_installedGames.erase(m_installedGames.begin());
    strSort::sortAZ(m_installedGames, &InstalledGameInfo::displayName, ascending);
    m_installedGames.insert(m_installedGames.begin(), vg);
}

std::vector<InstalledGameInfo>& GameManager::getInstalledGames() {
    if (m_installedGamesLoaded) return m_installedGames;

    auto tids = GameNACP::getInstalledGameTids();

    for (uint64_t tid : tids) {
        InstalledGameInfo info;
        info.appId = tid;

        int idx = findByAppId(tid);
        if (idx >= 0) {
            info.displayName = m_games[idx].displayName;
            info.version     = m_games[idx].version;
            info.modCount    = m_games[idx].modCount;
            info.iconId      = m_games[idx].iconId;
            info.isLoaded    = true;
        } else {
            info.displayName = format::appIdHex(tid);
            info.version     = "...";
            info.modCount    = "";
        }

        m_installedGames.push_back(std::move(info));
    }

    // 虚拟游戏置顶（appId=0 为哨兵，isLoaded=true 跳过 NACP 加载）
    InstalledGameInfo vg;
    vg.appId       = 0;
    vg.displayName = "";
    vg.version     = "--";
    vg.modCount    = "--";
    vg.isLoaded    = true;
    m_installedGames.insert(m_installedGames.begin(), vg);

    m_installedGamesLoaded = true;
    return m_installedGames;
}

void GameManager::setPendingFocus(uint64_t appId) {
    m_pendingFocusAppId = appId;
}

uint64_t GameManager::consumePendingFocus() {
    uint64_t appId = m_pendingFocusAppId;
    m_pendingFocusAppId = 0;
    return appId;
}
