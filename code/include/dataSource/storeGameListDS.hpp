/**
 * StoreGameListDS - 商店游戏列表数据源
 * 将 GameList 数据绑定到 RecyclingGrid 的 StoreGameCard Cell
 */

#pragma once

#include "view/recyclingGrid.hpp"
#include "view/storeGameCard.hpp"
#include "api/game.hpp"
#include <vector>
#include <functional>

class StoreGameListDS : public RecyclingGridDataSource {
public:
    /**
     * @brief 构造商店游戏列表数据源
     * @param storeGameList 游戏列表引用
     * @param clickCallback 卡片点击回调
     */
    StoreGameListDS(std::vector<api::game::GameList>& storeGameList, std::function<void(size_t)> clickCallback)
        : m_storeGameList(storeGameList), m_clickCallback(std::move(clickCallback)) {}

    /** @brief 返回游戏总数 */
    size_t getItemCount() override { return m_storeGameList.size(); }

    /**
     * @brief 取一个空卡片，填上第 index 个游戏的数据
     * @param grid 网格容器
     * @param index 游戏索引
     */
    RecyclingGridItem* cellForRow(RecyclingGrid* grid, size_t index) override {
        auto* card = static_cast<StoreGameCard*>(grid->dequeueReusableCell("StoreGameCard"));
        auto& game = m_storeGameList[index];
        card->setGame(game.gameName, std::to_string(game.modCount), game.lastUpdate);
        if (game.iconId > 0) card->setIcon(game.iconId);
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
    std::vector<api::game::GameList>& m_storeGameList;     // 游戏列表引用（与 StoreGameList 共享）
    std::function<void(size_t)> m_clickCallback;   // 卡片点击回调
};
