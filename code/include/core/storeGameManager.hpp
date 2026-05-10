/**
 * StoreGameManager - 商店游戏列表数据管理
 * 持有商店游戏列表和分页状态，纯数据层，不涉及 UI
 */

#pragma once

#include <vector>
#include <stop_token>
#include <unordered_set>
#include "api/game.hpp"
#include "api/url.hpp"
#include "utils/http.hpp"

enum class GameFilterMode { All, Installed, NotInstalled };

class StoreGameManager {
    
public:
    /**
     * @brief 获取下一页数据（子线程调用）
     * @param token 取消令牌
     */
    api::game::GameListResult fetchNextPage(std::stop_token token = {});

    /**
     * @brief 下载游戏图标数据（子线程调用）
     * @param gameTid 游戏 TID
     * @param token 取消令牌
     */
    http::Response fetchIcon(const std::string& gameTid, std::stop_token token);

    /**
     * @brief 追加一页数据（主线程调用）
     * @param result 数据结果
     */
    void appendPage(api::game::GameListResult result);

    /** @brief 获取游戏列表引用 */
    std::vector<api::game::GameList>& storeGameList();

    /**
     * @brief 设置搜索关键词
     * @param keyword 关键词
     */
    void setKeyword(const std::string& keyword);

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

private:
    std::vector<api::game::GameList> m_storeGameList;
    std::string m_keyword;
    int m_currentPage = 0;
    int m_total = 0;
    GameFilterMode m_filterMode = GameFilterMode::All;
    std::string m_tidsJson;
    std::unordered_set<uint64_t> m_installedTids;
};
