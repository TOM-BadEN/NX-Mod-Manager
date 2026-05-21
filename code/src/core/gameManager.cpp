/**
 * GameManager - 游戏数据管理
 */

#include "core/gameManager.hpp"
#include "common/config.hpp"
#include "utils/format.hpp"
#include "utils/textClean.hpp"
#include "utils/strSort.hpp"
#include <cstdlib>
#include <climits>
#include <unordered_map>

GameManager::GameManager() {
    m_jsonCache.load(config::gameInfoPath);

    // 确保 mods 目录和中转站目录存在（递归创建）
    fs::ensureDir(config::transitDir);

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

    loadSortSettings();
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

void GameManager::sort() { 
    sort(m_sortMode, m_sortAsc); 
}

void GameManager::setSortMode(SortMode mode) {
    if (m_sortMode == mode) return;
    m_sortMode = mode;
    switch (mode) {
        case SortMode::Name:       m_sortAsc = true;  Settings::setString("Sort", "mode", "Name"); break;
        case SortMode::ModCount:   m_sortAsc = false; Settings::setString("Sort", "mode", "ModCount"); break;
        case SortMode::RecentPlay: m_sortAsc = true;  Settings::setString("Sort", "mode", "RecentPlay"); break;
    }
    sort(m_sortMode, m_sortAsc);
}

void GameManager::toggleSortAsc() {
    m_sortAsc = !m_sortAsc;
    sort(m_sortMode, m_sortAsc);
}

SortMode GameManager::sortMode() const { 
    return m_sortMode; 
}

bool GameManager::sortAsc() const { 
    return m_sortAsc; 
}

void GameManager::loadSortSettings() {
    std::string modeStr = Settings::getString("Sort", "mode", "Name");
    if (modeStr == "ModCount") { m_sortMode = SortMode::ModCount; m_sortAsc = false; }
    else if (modeStr == "RecentPlay") { m_sortMode = SortMode::RecentPlay; m_sortAsc = true; }
    else { m_sortMode = SortMode::Name; m_sortAsc = true; }
    sort(m_sortMode, m_sortAsc);
}

void GameManager::sort(SortMode mode, bool ascending) {
    switch (mode) {
        case SortMode::Name:       sortByName(ascending); break;
        case SortMode::ModCount:   sortByModCount(ascending); break;
        case SortMode::RecentPlay: sortByRecentPlay(ascending); break;
    }
}

void GameManager::sortByName(bool ascending) {
    strSort::sortAZ(m_games, &GameInfo::displayName, &GameInfo::isFavorite, ascending);
}

void GameManager::sortByModCount(bool ascending) {
    auto cache = strSort::buildCache(m_games, &GameInfo::displayName);
    std::sort(m_games.begin(), m_games.end(),
        [ascending, &cache](const GameInfo& a, const GameInfo& b) {
            if (a.isFavorite != b.isFavorite) return a.isFavorite > b.isFavorite;
            int ca = std::stoi(a.modCount), cb = std::stoi(b.modCount);
            if (ca != cb) return ascending ? (ca < cb) : (ca > cb);
            return strSort::compareByCache(cache, a.displayName, b.displayName);
        });
}

void GameManager::sortByRecentPlay(bool ascending) {
    auto& tids = getInstalledTids();
    auto cache = strSort::buildCache(m_games, &GameInfo::displayName);
    std::sort(m_games.begin(), m_games.end(),
        [&tids, ascending, &cache](const GameInfo& a, const GameInfo& b) {
            if (a.isFavorite != b.isFavorite) return a.isFavorite > b.isFavorite;
            int posA = INT_MAX, posB = INT_MAX;
            for (int i = 0; i < static_cast<int>(tids.size()); i++) {
                if (tids[i] == a.appId) { posA = i; break; }
            }
            for (int i = 0; i < static_cast<int>(tids.size()); i++) {
                if (tids[i] == b.appId) { posB = i; break; }
            }
            if (posA != posB) return ascending ? (posA < posB) : (posA > posB);
            return strSort::compareByCache(cache, a.displayName, b.displayName);
        });
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

GameMetadata GameManager::fetchMetadata(int idx) {
    return gameNacp::getGameNACP(m_games[idx].appId);
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

    auto modDirs = fs::listSubDirs(game.dirPath);

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

    std::string englishName = gameNacp::getEnglishName(appId);
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
    sort();

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
    sort();

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
    sort();

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

        // 清理 modInfo.json: 删 installed
        JsonFile modJson;
        if (modJson.load(game.dirPath + config::modInfoFile)) {
            auto modDirs = fs::listSubDirs(game.dirPath);
            for (const auto& dir : modDirs) {
                modJson.removeKey(dir, "installed");
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

}

void GameManager::saveJsonCache() {
    m_jsonCache.save();
}

GameMetadata GameManager::fetchInstalledMetadata(size_t idx) {
    if (idx >= m_installedGames.size()) return {};
    return gameNacp::getGameNACP(m_installedGames[idx].appId);
}

void GameManager::sortInstalledGames(bool ascending) {
    if (m_installedGames.empty()) return;
    // 弹出虚拟游戏（appId==0 的头部条目），按最近游玩顺序排序真实游戏，再插回头部
    InstalledGameInfo vg = m_installedGames.front();
    m_installedGames.erase(m_installedGames.begin());
    auto& tids = getInstalledTids();
    std::sort(m_installedGames.begin(), m_installedGames.end(),
        [&tids, ascending](const InstalledGameInfo& a, const InstalledGameInfo& b) {
            int posA = INT_MAX, posB = INT_MAX;
            for (int i = 0; i < static_cast<int>(tids.size()); i++) {
                if (tids[i] == a.appId) { posA = i; break; }
            }
            for (int i = 0; i < static_cast<int>(tids.size()); i++) {
                if (tids[i] == b.appId) { posB = i; break; }
            }
            return ascending ? (posA < posB) : (posA > posB);
        });
    m_installedGames.insert(m_installedGames.begin(), vg);
}

const std::vector<uint64_t>& GameManager::getInstalledTids() {
    if (m_installedTids.empty()) m_installedTids = gameNacp::getInstalledGameTids();
    return m_installedTids;
}

std::vector<InstalledGameInfo>& GameManager::getInstalledGames() {
    if (m_installedGamesLoaded) return m_installedGames;

    auto& tids = getInstalledTids();

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

void GameManager::setPendingRemove(uint64_t appId) {
    m_pendingRemoveAppId = appId;
}

uint64_t GameManager::consumePendingRemove() {
    uint64_t id = m_pendingRemoveAppId;
    m_pendingRemoveAppId = 0;
    return id;
}
