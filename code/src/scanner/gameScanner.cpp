/**
 * GameScanner - 主页游戏扫描
 */

#include "scanner/gameScanner.hpp"
#include "utils/fsHelper.hpp"
#include "utils/jsonFile.hpp"
#include "common/config.hpp"
#include "utils/strSort.hpp"
#include <algorithm>
#include <cstdlib>
#include <cstdio>

std::vector<GameInfo> GameScanner::scanGames(JsonFile& jsonCache) {
    std::vector<GameInfo> games;

    // 确保 mods 目录存在
    fs::ensureDir(config::modsRoot);

    // 扫描 /mods2/ 获取所有游戏目录名
    auto dirs = fs::listSubDirs(config::modsRoot);

    for (const auto& dirName : dirs) {
        std::string dirPath = std::string(config::modsRoot) + dirName + "/";

        // 找 hex 子目录 → appId
        auto subDirs = fs::listSubDirs(dirPath);
        uint64_t appId = 0;
        std::string appIdHex;

        for (const auto& sub : subDirs) {
            char* end = nullptr;
            uint64_t val = std::strtoull(sub.c_str(), &end, 16);
            // 检查是否为有效游戏 ID（非零，且高 32 位以 0x01 开头）
            uint32_t highPart = static_cast<uint32_t>(val >> 32);
            bool isGameId = (highPart & 0xFF000000) == 0x01000000;
            if (end != sub.c_str() && *end == '\0' && val != 0 && isGameId) {
                appId = val;
                appIdHex = sub;
                break;
            }
        }

        if (appId == 0) continue;

        // 数 mod（子目录 + .zip 文件）
        std::string appIdPath = dirPath + appIdHex + "/";
        int modCount = fs::countItems(appIdPath, config::modFileExts);

        // 用 appId 查 JSON
        char appIdKey[17];
        std::snprintf(appIdKey, sizeof(appIdKey), "%016lX", appId);

        std::string displayName = jsonCache.getString(appIdKey, "displayName");
        std::string gameName = jsonCache.getString(appIdKey, "gameName");
        std::string version = jsonCache.getString(appIdKey, "version");
        bool isFavorite = jsonCache.getBool(appIdKey, "favorite", false);

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

        games.push_back(std::move(info));
    }

    // 首次预排序（支持拼音）
    strSort::sortAZ(games, &GameInfo::displayName, &GameInfo::isFavorite);
    return games;
}
