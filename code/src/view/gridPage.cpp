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
        
        m_cards[i]->getFocusEvent()->subscribe([this, i](brls::View*) {
            m_indexUpdate.update(m_currentPage * CARDS_PER_PAGE + i, m_games.size());
        });
    }
    
    // 注册 LB/RB 翻页
    registerAction("上一页", brls::BUTTON_LB, [this](...) { prevPage(); return true; }, true, true);
    registerAction("下一页", brls::BUTTON_RB, [this](...) { nextPage(); return true; }, true, true);
}

// 设置数据源
void GridPage::setGameList(const std::vector<GameInfo>& games) {
    m_games = games;
    m_currentPage = 0;
    refreshPage();
}

// 下一页
void GridPage::nextPage() {
    if (m_currentPage < getTotalPages() - 1) {
        m_currentPage++;
        refreshPage();
    } else {
        // 已是最后一页，先跳到最后一个可见卡片，已在则抖动
        int focused = findFocusedCardIndex();
        for (int i = CARDS_PER_PAGE - 1; i > focused; i--) {
            if (isCardVisible(i)) {
                brls::Application::giveFocus(m_cards[i]);
                m_indexUpdate.update(m_currentPage * CARDS_PER_PAGE + i, m_games.size());
                return;
            }
        }
        auto* focus = brls::Application::getCurrentFocus();
        if (focus) focus->shakeHighlight(brls::FocusDirection::RIGHT);
    }
}

// 上一页
void GridPage::prevPage() {
    if (m_currentPage > 0) {
        m_currentPage--;
        refreshPage();
    } else {
        // 已是第一页，先跳到第一个卡片，已在则抖动
        int focused = findFocusedCardIndex();
        if (focused > 0 && isCardVisible(0)) {
            brls::Application::giveFocus(m_cards[0]);
            m_indexUpdate.update(m_currentPage * CARDS_PER_PAGE, m_games.size());
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

// 刷新当前页的 9 张卡片
void GridPage::refreshPage() {
    int startIndex = m_currentPage * CARDS_PER_PAGE;
    
    for (int i = 0; i < CARDS_PER_PAGE; i++) {
        int dataIndex = startIndex + i;
        if (dataIndex < static_cast<int>(m_games.size())) {
            auto& game = m_games[dataIndex];
            m_cards[i]->setGame(game.name, game.version, game.modCount);
            if (game.iconId > 0) m_cards[i]->setIcon(game.iconId);
        }
        else {
            m_cards[i]->clear();
        }
    }
    
    // 翻页后更新索引并处理焦点
    int focusedCard = findFocusedCardIndex();
    if (focusedCard >= 0 && !isCardVisible(focusedCard)) {
        for (int i = focusedCard - 1; i >= 0; i--) {
            if (isCardVisible(i)) {
                brls::Application::giveFocus(m_cards[i]);
                focusedCard = i;
                break;
            }
        }
    }
    if (focusedCard >= 0) {
        m_indexUpdate.update(m_currentPage * CARDS_PER_PAGE + focusedCard, m_games.size());
    }
}

// 查找当前聚焦的卡片索引（0~8），未找到返回 -1
int GridPage::findFocusedCardIndex() {
    brls::View* focused = brls::Application::getCurrentFocus();
    for (int i = 0; i < CARDS_PER_PAGE; i++) {
        if (m_cards[i] == focused || m_cards[i]->isChildFocused())
            return i;
    }
    return -1;
}

// 卡片是否可见
bool GridPage::isCardVisible(int index) {
    if (index < 0 || index >= CARDS_PER_PAGE) return false;
    
    auto visibility = m_cards[index]->getVisibility();
    return visibility == brls::Visibility::VISIBLE;
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
            if (col < 2 && isCardVisible(cardIndex + 1))
                targetIndex = cardIndex + 1;                    // 同行右移
            else if (row < 2 && isCardVisible((row + 1) * 3))
                targetIndex = (row + 1) * 3;                   // 换行到下一行第一个
            else if (m_currentPage < getTotalPages() - 1) {
                nextPage();
                targetIndex = 0;
            }
            break;
            
        case brls::FocusDirection::LEFT:
            if (col > 0)
                targetIndex = cardIndex - 1;                    // 同行左移
            else if (row > 0)
                targetIndex = (row - 1) * 3 + 2;               // 换行到上一行最后一个
            else if (m_currentPage > 0) {
                prevPage();
                targetIndex = 8;
            }
            break;
            
        case brls::FocusDirection::DOWN:
            if (row < 2 && isCardVisible(cardIndex + 3))
                targetIndex = cardIndex + 3;                    // 同列下移
            else if (m_currentPage < getTotalPages() - 1) {
                nextPage();
                targetIndex = col;
            } else {
                // 回退：找当前卡片之后最近的可见卡片
                for (int i = cardIndex + 1; i < CARDS_PER_PAGE; i++) {
                    if (isCardVisible(i)) { targetIndex = i; break; }
                }
            }
            break;
            
        case brls::FocusDirection::UP:
            if (row > 0)
                targetIndex = cardIndex - 3;                    // 同列上移
            else if (m_currentPage > 0) {
                prevPage();
                targetIndex = 6 + col;
            } else {
                // 回退：找当前卡片之前最近的可见卡片
                for (int i = cardIndex - 1; i >= 0; i--) {
                    if (isCardVisible(i)) { targetIndex = i; break; }
                }
            }
            break;
    }
    
    // 目标卡片有效且可见，更新索引并返回
    if (targetIndex >= 0 && isCardVisible(targetIndex)) {
        m_indexUpdate.update(m_currentPage * CARDS_PER_PAGE + targetIndex, m_games.size());
        return m_cards[targetIndex];
    }
    
    // 目标卡片不可见（最后一页不满），向左找最近的可见卡片
    if (targetIndex >= 0) {
        for (int i = targetIndex - 1; i >= 0; i--) {
            if (isCardVisible(i)) {
                m_indexUpdate.update(m_currentPage * CARDS_PER_PAGE + i, m_games.size());
                return m_cards[i];
            }
        }
    }
    
    return nullptr;
}

// 工厂函数：用于 XML 注册
brls::View* GridPage::create() {
    return new GridPage();
}
