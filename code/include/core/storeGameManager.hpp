/**
 * StoreGameManager - 商店游戏列表数据管理
 * 持有商店游戏列表和分页状态，纯数据层，不涉及 UI
 */

#pragma once

#include <vector>
#include <unordered_set>
#include "api/game.hpp"

enum class GameFilterMode { All, Installed, NotInstalled };

class StoreGameManager {
    
public:
    /**
     * @brief 追加一页数据（主线程调用）
     * @param result 数据结果
     */
    void appendPage(api::game::GameListResult result);

    /** @brief 获取游戏列表引用 */
    std::vector<api::game::GameList>& storeGameList();

    /** @brief 缓存本机已安装游戏 TID（预拼 JSON + 构建 hash set） */
    void cacheInstalledTids();

    /** @brief 设置筛选模式 */
    void setFilterMode(GameFilterMode mode);

    /** @brief 获取当前筛选模式 */
    GameFilterMode getFilterMode() const;

    /** @brief 重置分页状态（清空列表 + 归零页码） */
    void reset();

    /** @brief 获取总数 */
    int total() const;

    /** @brief 是否还有更多数据 */
    bool hasMore() const;

    /**
     * @brief 获取已安装游戏的本地版本号
     * @param index 游戏索引
     */
    std::string getLocalVersion(size_t index);

    /** @brief 获取当前页码 */
    int currentPage() const;

    /** @brief 获取已安装 TID 的 JSON 字符串 */
    const std::string& tidsJson() const;

private:
    std::vector<api::game::GameList> m_storeGameList;
    int m_currentPage = 0;
    int m_total = 0;
    GameFilterMode m_filterMode = GameFilterMode::All;
    std::string m_tidsJson;
    std::unordered_set<uint64_t> m_installedTids;
};
