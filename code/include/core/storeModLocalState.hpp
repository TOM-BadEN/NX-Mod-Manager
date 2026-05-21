/**
 * StoreModLocalState - 商店模组本地下载状态
 * 从当前游戏的本地 mod 数据中收集商店 modID，用于判断商店 mod 是否已下载
 */

#pragma once

#include <string>
#include <unordered_set>
#include "core/gameManager.hpp"
#include "core/modManager.hpp"

class StoreModLocalState {
public:
    /**
     * @brief 从当前 ModManager 加载已下载的商店 modID
     * @param modManager 当前游戏的 ModManager
     */
    void loadFromModManager(ModManager& modManager);

    /**
     * @brief 从 GameManager 中对应游戏的 modInfo.json 加载已下载的商店 modID
     * @param gameManager 游戏数据管理
     * @param gameTid 游戏 TID
     */
    void loadFromGame(GameManager& gameManager, const std::string& gameTid);

    /**
     * @brief 判断商店 mod 是否已下载
     * @param modId 商店模组 ID
     */
    bool isDownloaded(int modId) const;

    /**
     * @brief 标记商店 mod 已下载
     * @param modId 商店模组 ID
     */
    void markDownloaded(int modId);

private:
    std::unordered_set<int> m_downloadedModIds;
};
