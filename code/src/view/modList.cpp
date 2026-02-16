/**
 * ModList - Mod 列表组件
 * 4 个 ModCard 纵向排列，逐行滚动
 */

#include "view/modList.hpp"

ModList::ModList() {
    inflateFromXMLRes("xml/view/modList.xml");

    // 获取 4 张卡片的引用，并监听焦点事件更新索引
    for (int i = 0; i < CARDS_PER_PAGE; i++) {
        std::string id = "modList/card" + std::to_string(i);
        m_cards[i] = dynamic_cast<ModCard*>(getView(id));
        auto* focusEvent = m_cards[i]->getFocusEvent();
        focusEvent->subscribe([this, i](brls::View*) {
            if (m_mods) m_indexUpdate.update(m_scrollOffset + i, m_mods->size());
        });
    }
}

// 设置数据源
void ModList::setModList(std::vector<ModInfo>& mods) {
    m_mods = &mods;
    m_scrollOffset = 0;
    refreshItems();
}

// 数据源已变更，重新加载（不重置滚动位置）
void ModList::reloadData() {
    refreshItems();
}

// 刷新 4 个槽位
void ModList::refreshItems() {
    for (int i = 0; i < CARDS_PER_PAGE; i++) {
        int dataIndex = m_scrollOffset + i;
        if (m_mods && dataIndex < static_cast<int>(m_mods->size())) {
            auto& mod = (*m_mods)[dataIndex];
            m_cards[i]->setMod(mod.displayName, modTypeText(mod.type), mod.isInstalled);
        } else {
            m_cards[i]->clear();
        }
    }
}

// 自定义导航（逐行滚动）
brls::View* ModList::getNextFocus(brls::FocusDirection direction, brls::View* currentView) {
    int cardIndex = findFocusedCardIndex();
    if (cardIndex == -1) return brls::Box::getNextFocus(direction, currentView);

    int total = m_mods ? static_cast<int>(m_mods->size()) : 0;

    switch (direction) {
        case brls::FocusDirection::DOWN:
            // 下一个槽位有数据 → 移动焦点
            if (cardIndex < CARDS_PER_PAGE - 1 && isCardVisible(cardIndex + 1)) {
                m_indexUpdate.update(m_scrollOffset + cardIndex + 1, total);
                return m_cards[cardIndex + 1];
            }
            // 已在最后一个槽位，还有更多数据 → 滚动
            if (cardIndex == CARDS_PER_PAGE - 1 && m_scrollOffset + CARDS_PER_PAGE < total) {
                m_scrollOffset++;
                refreshItems();
                m_indexUpdate.update(m_scrollOffset + cardIndex, total);
                return m_cards[cardIndex];
            }
            break;

        case brls::FocusDirection::UP:
            // 上一个槽位 → 移动焦点
            if (cardIndex > 0) {
                m_indexUpdate.update(m_scrollOffset + cardIndex - 1, total);
                return m_cards[cardIndex - 1];
            }
            // 已在第一个槽位，还有更多数据 → 滚动
            if (cardIndex == 0 && m_scrollOffset > 0) {
                m_scrollOffset--;
                refreshItems();
                m_indexUpdate.update(m_scrollOffset, total);
                return m_cards[0];
            }
            break;

        default:
            break;
    }

    return nullptr;
}

void ModList::setIndexChangeCallback(std::function<void(int, int)> callback) {
    m_indexUpdate.setCallback(callback);
}

int ModList::findFocusedCardIndex() {
    brls::View* focused = brls::Application::getCurrentFocus();
    for (int i = 0; i < CARDS_PER_PAGE; i++) {
        if (m_cards[i] == focused || m_cards[i]->isChildFocused()) return i;
    }
    return -1;
}

bool ModList::isCardVisible(int index) {
    if (index < 0 || index >= CARDS_PER_PAGE) return false;
    return m_cards[index]->getVisibility() == brls::Visibility::VISIBLE;
}

// 工厂函数
brls::View* ModList::create() {
    return new ModList();
}
