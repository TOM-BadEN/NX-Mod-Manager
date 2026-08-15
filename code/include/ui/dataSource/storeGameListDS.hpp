/**
 * StoreGameListDS - 商店游戏列表数据源
 * 将 GameList 数据绑定到 RecyclingGrid 的 StoreGameCard Cell
 */

#pragma once

#include "api/game.hpp"
#include "ui/view/recyclingGrid.hpp"
#include "ui/view/storeGameCard.hpp"
#include <borealis/core/cache_helper.hpp>
#include <functional>
#include <string>
#include <utility>
#include <vector>

class StoreGameListDS : public RecyclingGridDataSource {
public:
    /**
     * @brief 构造商店游戏列表数据源
     * @param storeGameList 游戏列表引用
     * @param skeletonCount 游戏列表为空时显示的轻量骨架数量
     * @param textureMissingCallback 纹理缓存失效回调
     * @param clickCallback 卡片点击回调
     */
    StoreGameListDS(std::vector<api::game::GameList>& storeGameList, size_t skeletonCount, std::function<void(std::string)> textureMissingCallback, std::function<void(size_t)> clickCallback)
        : m_storeGameList(storeGameList), m_skeletonCount(skeletonCount), m_textureMissingCallback(std::move(textureMissingCallback)), m_clickCallback(std::move(clickCallback)) {}

    /** @brief 返回游戏总数 */
    size_t getItemCount() override { return m_storeGameList.empty() ? m_skeletonCount : m_storeGameList.size(); }

    /**
     * @brief 取一个空卡片，填上第 index 个游戏的数据
     * @param grid 网格容器
     * @param index 游戏索引
     */
    RecyclingGridItem* cellForRow(RecyclingGrid* grid, size_t index) override {
        if (m_storeGameList.empty()) return grid->dequeueReusableCell("Skeleton");

        auto& game = m_storeGameList[index];
        if (game.isPending) return grid->dequeueReusableCell("Skeleton");

        int iconId = 0;
        if (!game.iconKey.empty()) {
            iconId = brls::TextureCache::instance().getCache(game.iconKey);
            if (iconId <= 0) {
                m_textureMissingCallback(game.iconKey);
                return grid->dequeueReusableCell("Skeleton");
            }
        }

        auto* card = static_cast<StoreGameCard*>(grid->dequeueReusableCell("StoreGameCard"));
        card->setGame(game.gameName, std::to_string(game.modCount), game.lastUpdate);
        if (iconId > 0) card->setIcon(iconId);
        card->setInstalled(game.installed);
        return card;
    }

    /**
     * @brief 用户点击卡片时触发
     * @param grid 网格容器
     * @param index 游戏索引
     */
    void onItemSelected(RecyclingGrid* grid, size_t index) override {
        if (m_clickCallback) m_clickCallback(index);
    }

    /** @brief 清空数据（空实现） */
    void clearData() override {}

private:
    std::vector<api::game::GameList>& m_storeGameList;         // 游戏列表引用（与 StoreGameList 共享）
    size_t m_skeletonCount;                                    // 游戏列表为空时显示的轻量骨架数量
    std::function<void(std::string)> m_textureMissingCallback; // 纹理缓存失效回调
    std::function<void(size_t)> m_clickCallback;                // 卡片点击回调
};
