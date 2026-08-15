/**
 * InstalledGameDS - 添加游戏页面的卡片数据源
 * 将 InstalledGameInfo 数据绑定到 RecyclingGrid 的 AddGameCard Cell
 *
 * Cell（单元格）：屏幕上可见的卡片 View 实例，数量固定（约 20 个）。
 * 滚动时不销毁，而是擦掉旧数据、填入新数据（回收复用）。
 */

#pragma once

#include "common/gameInfo.hpp"
#include "ui/view/addGameCard.hpp"
#include "ui/view/recyclingGrid.hpp"
#include <borealis/core/cache_helper.hpp>
#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

class InstalledGameDS : public RecyclingGridDataSource {
public:
    /**
     * @brief 构造已安装游戏卡片数据源
     * @param games 已安装游戏列表引用
     * @param textureMissingCallback 纹理缓存失效回调
     * @param clickCallback 卡片点击回调
     */
    InstalledGameDS(std::vector<InstalledGameInfo>& games, std::function<void(size_t)> textureMissingCallback, std::function<void(size_t)> clickCallback)
        : m_games(games), m_textureMissingCallback(std::move(textureMissingCallback)), m_clickCallback(std::move(clickCallback)) {}

    /**
     * @brief 返回游戏总数
     * @return 游戏总数
     */
    size_t getItemCount() override { return m_games.size(); }

    /**
     * @brief 取一个空卡片，填上第 index 个游戏的数据
     * @param grid 网格容器
     * @param index 游戏索引
     * @return 已绑定游戏数据的卡片
     */
    RecyclingGridItem* cellForRow(RecyclingGrid* grid, size_t index) override {
        auto& game = m_games[index];
        if (game.isPending) return grid->dequeueReusableCell("Skeleton");

        int iconId = 0;
        if (!game.iconKey.empty()) {
            iconId = brls::TextureCache::instance().getCache(game.iconKey);
            if (iconId <= 0) {
                m_textureMissingCallback(index);
                return grid->dequeueReusableCell("Skeleton");
            }
        }

        auto* card = static_cast<AddGameCard*>(grid->dequeueReusableCell("AddGameCard"));
        card->setGame(game.displayName, game.version, game.modCount);
        if (iconId > 0) card->setIcon(iconId);
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
    std::vector<InstalledGameInfo>& m_games;              // 已安装游戏列表引用
    std::function<void(size_t)> m_textureMissingCallback; // 纹理缓存失效回调
    std::function<void(size_t)> m_clickCallback;          // 卡片点击回调
};
