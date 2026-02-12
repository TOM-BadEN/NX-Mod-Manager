/**
 * GridPage - 九宫格翻页组件
 * 3x3 布局显示 GameCard，支持 LB/RB 翻页
 */

#include "view/gridPage.hpp"

// 构造函数：加载布局 + 注册翻页按键
GridPage::GridPage() {
    inflateFromXMLRes("xml/view/gridPage.xml");
    
    // 获取 9 张卡片的引用，并监听焦点事件更新索引
    for (int i = 0; i < CARDS_PER_PAGE; i++) {
        std::string id = "gridPage/card" + std::to_string(i);
        m_cards[i] = dynamic_cast<GameCard*>(getView(id));
        auto* focusEvent = m_cards[i]->getFocusEvent();
        focusEvent->subscribe([this, i](brls::View*) {
            m_indexUpdate.update(m_currentPage * CARDS_PER_PAGE + i, m_games.size());
        });
    }
    
    // 注册 LB/RB 翻页
    registerAction("上一页", brls::BUTTON_LB, [this](...) {
        prevPage();
        return true;
    }, true, true);
    
    registerAction("下一页", brls::BUTTON_RB, [this](...) {
        nextPage();
        return true;
    }, true, true);
}

// 设置数据源
void GridPage::setGameList(const std::vector<GameInfo>& games) {
    m_games = games;
    m_currentPage = 0;
    refreshPage();
}

// 下一页（LB/RB 调用）
void GridPage::nextPage() {
    if (m_currentPage < getTotalPages() - 1) {
        flipPage(1);
        fixFocusAfterFlip();
    } else {
        // 已是最后一页，先跳到最后一个可见卡片，已在则抖动
        int focused = findFocusedCardIndex();
        for (int i = CARDS_PER_PAGE - 1; i > focused; i--) {
            if (isCardVisible(i)) {
                focusCard(i);
                return;
            }
        }
        auto* focus = brls::Application::getCurrentFocus();
        if (focus) focus->shakeHighlight(brls::FocusDirection::RIGHT);
    }
}

// 上一页（LB/RB 调用）
void GridPage::prevPage() {
    if (m_currentPage > 0) {
        flipPage(-1);
        fixFocusAfterFlip();
    } else {
        // 已是第一页，先跳到第一个卡片，已在则抖动
        int focused = findFocusedCardIndex();
        if (focused > 0 && isCardVisible(0)) {
            focusCard(0);
            return;
        }
        auto* focus = brls::Application::getCurrentFocus();
        if (focus) focus->shakeHighlight(brls::FocusDirection::LEFT);
    }
}

int GridPage::getTotalPages() {
    if (m_games.empty()) return 1;
    return (m_games.size() + CARDS_PER_PAGE - 1) / CARDS_PER_PAGE;
}

void GridPage::setIndexChangeCallback(std::function<void(int, int)> callback) {
    m_indexUpdate.setCallback(callback);
}

// 刷新当前页的 9 张卡片（只刷数据，不处理焦点）
void GridPage::refreshPage() {
    int startIndex = m_currentPage * CARDS_PER_PAGE;
    for (int i = 0; i < CARDS_PER_PAGE; i++) {
        int dataIndex = startIndex + i;
        if (dataIndex < static_cast<int>(m_games.size())) {
            auto& game = m_games[dataIndex];
            m_cards[i]->setGame(game.name, game.version, game.modCount);
            if (game.iconId > 0) m_cards[i]->setIcon(game.iconId);
        } else {
            m_cards[i]->clear();
        }
    }
}

// 纯翻页：改页码 + 刷新数据
void GridPage::flipPage(int delta) {
    m_currentPage += delta;
    refreshPage();
}

// 焦点转移 + 索引更新
void GridPage::focusCard(int cardIndex) {
    brls::Application::giveFocus(m_cards[cardIndex]);
    m_indexUpdate.update(m_currentPage * CARDS_PER_PAGE + cardIndex, m_games.size());
}

// 翻页后修正不可见焦点
void GridPage::fixFocusAfterFlip() {
    int focused = findFocusedCardIndex();
    if (focused >= 0 && !isCardVisible(focused)) focused = findVisibleCard(focused - 1, -1);
    if (focused >= 0) focusCard(focused);
}

// 查找当前聚焦的卡片索引（0~8），未找到返回 -1
int GridPage::findFocusedCardIndex() {
    brls::View* focused = brls::Application::getCurrentFocus();
    for (int i = 0; i < CARDS_PER_PAGE; i++) {
        if (m_cards[i] == focused || m_cards[i]->isChildFocused()) return i;
    }
    return -1;
}

// 卡片是否可见
bool GridPage::isCardVisible(int index) {
    if (index < 0 || index >= CARDS_PER_PAGE) return false;
    return m_cards[index]->getVisibility() == brls::Visibility::VISIBLE;
}

// 从 start 按 step 方向查找最近的可见卡片，未找到返回 -1
int GridPage::findVisibleCard(int start, int step) {
    for (int i = start; i >= 0 && i < CARDS_PER_PAGE; i += step) {
        if (isCardVisible(i)) return i;
    }
    return -1;
}

// 自定义导航逻辑
brls::View* GridPage::getNextFocus(brls::FocusDirection direction, brls::View* currentView) {
    int cardIndex = findFocusedCardIndex();
    if (cardIndex == -1) return brls::Box::getNextFocus(direction, currentView);
    
    int row = cardIndex / 3;
    int col = cardIndex % 3;
    int targetIndex = -1;
    
    switch (direction) {
        case brls::FocusDirection::RIGHT:
            if (col < 2 && isCardVisible(cardIndex + 1)) targetIndex = cardIndex + 1;
            else if (row < 2 && isCardVisible((row + 1) * 3)) targetIndex = (row + 1) * 3;
            else if (m_currentPage < getTotalPages() - 1) {
                flipPage(1);
                targetIndex = 0;
            }
            break;
            
        case brls::FocusDirection::LEFT:
            if (col > 0) targetIndex = cardIndex - 1;
            else if (row > 0) targetIndex = (row - 1) * 3 + 2;
            else if (m_currentPage > 0) {
                flipPage(-1);
                targetIndex = 8;
            }
            break;
            
        case brls::FocusDirection::DOWN:
            if (row < 2 && isCardVisible(cardIndex + 3)) targetIndex = cardIndex + 3;
            else if (m_currentPage < getTotalPages() - 1) {
                flipPage(1);
                targetIndex = col;
                // 翻到不满页时 col 可能不可见，回退找最近可见卡片
                if (!isCardVisible(targetIndex)) targetIndex = findVisibleCard(targetIndex - 1, -1);
            } else {
                targetIndex = findVisibleCard(cardIndex + 1, 1);
            }
            break;
            
        case brls::FocusDirection::UP:
            if (row > 0) targetIndex = cardIndex - 3;
            else if (m_currentPage > 0) {
                flipPage(-1);
                targetIndex = 6 + col;
            } else {
                targetIndex = findVisibleCard(cardIndex - 1, -1);
            }
            break;
    }
    
    if (targetIndex >= 0 && isCardVisible(targetIndex)) {
        m_indexUpdate.update(m_currentPage * CARDS_PER_PAGE + targetIndex, m_games.size());
        return m_cards[targetIndex];
    }
    
    return nullptr;
}

// 工厂函数：用于 XML 注册
brls::View* GridPage::create() {
    return new GridPage();
}
