/**
 * ModList - Mod 列表组件
 * 4 个 ModCard 纵向排列，逐行滚动
 */

#pragma once

#include <borealis.hpp>
#include <vector>
#include "view/modCard.hpp"
#include "utils/indexUpdate.hpp"
#include "common/modInfo.hpp"

class ModList : public brls::Box {
public:
    ModList();

    // 自定义导航（上下滚动）
    brls::View* getNextFocus(brls::FocusDirection direction, brls::View* currentView) override;

    // 设置数据源
    void setModList(std::vector<ModInfo>& mods);

    // 数据源已变更，重新加载（不重置滚动位置）
    void reloadData();

    // 索引更新回调
    void setIndexChangeCallback(std::function<void(int, int)> callback);

    // 工厂函数
    static brls::View* create();

private:
    static constexpr int CARDS_PER_PAGE = 4;

    ModCard* m_cards[CARDS_PER_PAGE];
    std::vector<ModInfo>* m_mods = nullptr;
    int m_scrollOffset = 0;
    IndexUpdate m_indexUpdate;

    void refreshItems();
    int findFocusedCardIndex();
    bool isCardVisible(int index);
};
