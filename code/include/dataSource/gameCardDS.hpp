/**
 * GameCardDS - 主页游戏卡片数据源
 * 将 m_games 数据绑定到 RecyclingGrid 的 GameCard Cell
 *
 * Cell（单元格）：屏幕上可见的卡片 View 实例，数量固定（约 20 个）。
 * 滚动时不销毁，而是擦掉旧数据、填入新数据（回收复用）。
 */

#pragma once

#include "view/recyclingGrid.hpp"
#include "view/gameCard.hpp"
#include "common/gameInfo.hpp"
#include <vector>
#include <functional>

class GameCardDS : public RecyclingGridDataSource {
public:
    GameCardDS(std::vector<GameInfo>& games, std::function<void(size_t)> clickCallback)
        : m_games(games), m_clickCallback(std::move(clickCallback)) {}

    // 返回游戏总数
    size_t getItemCount() override { return m_games.size(); }

    // 取一个空卡片，填上第 index 个游戏的数据，交给网格显示
    RecyclingGridItem* cellForRow(RecyclingGrid* grid, size_t index) override {
        auto* card = static_cast<GameCard*>(grid->dequeueReusableCell("GameCard"));
        auto& game = m_games[index];
        card->setGame(game.displayName, game.version, game.modCount);
        if (game.iconId > 0) card->setIcon(game.iconId);
        return card;
    }

    // 用户点击第 index 个卡片时触发
    void onItemSelected(RecyclingGrid* grid, size_t index) override {
        if (m_clickCallback) m_clickCallback(index);
    }

    // 清空数据
    void clearData() override {}

private:
    std::vector<GameInfo>& m_games;                  // 游戏列表引用（与 Home 共享）
    std::function<void(size_t)> m_clickCallback;     // 卡片点击回调
};
