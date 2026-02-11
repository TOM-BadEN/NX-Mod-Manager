/**
 * GridPage - 九宫格翻页组件
 * 3x3 布局显示 GameCard，支持 LB/RB 翻页
 */

#include "view/gridPage.hpp"

// 构造函数：加载布局 + 注册翻页按键
GridPage::GridPage()
{
    inflateFromXMLRes("xml/view/gridPage.xml");
    
    // 获取 9 张卡片的引用
    for (int i = 0; i < CARDS_PER_PAGE; i++)
    {
        std::string id = "gridPage/card" + std::to_string(i);
        m_cards[i] = dynamic_cast<GameCard*>(getView(id));
    }
    
    // 注册 LB/RB 翻页
    registerAction("上一页", brls::BUTTON_LB, [this](...) { prevPage(); return true; }, true);
    registerAction("下一页", brls::BUTTON_RB, [this](...) { nextPage(); return true; }, true);
}

// 设置数据源
void GridPage::setGameList(const std::vector<GameInfo>& games)
{
    m_games = games;
    m_currentPage = 0;
    refreshPage();
}

// 下一页
void GridPage::nextPage()
{
    if (m_currentPage < getTotalPages() - 1)
    {
        m_currentPage++;
        refreshPage();
    }
}

// 上一页
void GridPage::prevPage()
{
    if (m_currentPage > 0)
    {
        m_currentPage--;
        refreshPage();
    }
}

int GridPage::getCurrentPage() { 
    return m_currentPage + 1; 
}

int GridPage::getTotalPages()
{
    if (m_games.empty()) return 1;
    return (m_games.size() + CARDS_PER_PAGE - 1) / CARDS_PER_PAGE;
}

void GridPage::setPageChangeCallback(std::function<void(int current, int total)> callback)
{
    m_pageChangeCallback = callback;
}

// 刷新当前页的 9 张卡片
void GridPage::refreshPage()
{
    int startIndex = m_currentPage * CARDS_PER_PAGE;
    
    for (int i = 0; i < CARDS_PER_PAGE; i++)
    {
        int dataIndex = startIndex + i;
        if (dataIndex < static_cast<int>(m_games.size()))
        {
            auto& game = m_games[dataIndex];
            m_cards[i]->setGame(game.name, game.version, game.modCount);
            if (game.iconId > 0) m_cards[i]->setIcon(game.iconId);
        }
        else
        {
            m_cards[i]->clear();
        }
    }
    
    // 通知外部更新页码
    if (m_pageChangeCallback)
        m_pageChangeCallback(getCurrentPage(), getTotalPages());
}

// 查找当前聚焦的卡片索引（0~8），未找到返回 -1
int GridPage::findFocusedCardIndex()
{
    brls::View* focused = brls::Application::getCurrentFocus();
    for (int i = 0; i < CARDS_PER_PAGE; i++)
    {
        // 检查卡片本身或其子视图是否拥有焦点
        if (m_cards[i] == focused || m_cards[i]->isChildFocused())
            return i;
    }
    return -1;
}

// 卡片是否可见
bool GridPage::isCardVisible(int index)
{
    if (index < 0 || index >= CARDS_PER_PAGE)
        return false;
    
    auto visibility = m_cards[index]->getVisibility();
    return visibility == brls::Visibility::VISIBLE;
}

// 自定义导航逻辑
brls::View* GridPage::getNextFocus(brls::FocusDirection direction, brls::View* currentView)
{
    int cardIndex = findFocusedCardIndex();
    if (cardIndex == -1)
        return brls::Box::getNextFocus(direction, currentView);
    
    int row = cardIndex / 3;
    int col = cardIndex % 3;
    int targetIndex = -1;
    
    switch (direction)
    {
        case brls::FocusDirection::RIGHT:
            if (col < 2 && isCardVisible(cardIndex + 1))
                targetIndex = cardIndex + 1;                    // 同行右移
            else if (row < 2 && isCardVisible((row + 1) * 3))
                targetIndex = (row + 1) * 3;                   // 换行到下一行第一个
            else if (m_currentPage < getTotalPages() - 1)
            {
                nextPage();                                     // 翻到下一页
                targetIndex = 0;
            }
            break;
            
        case brls::FocusDirection::LEFT:
            if (col > 0)
                targetIndex = cardIndex - 1;                    // 同行左移
            else if (row > 0)
                targetIndex = (row - 1) * 3 + 2;               // 换行到上一行最后一个
            else if (m_currentPage > 0)
            {
                prevPage();                                     // 翻到上一页
                targetIndex = 8;                                // 上一页最后一个
            }
            break;
            
        case brls::FocusDirection::DOWN:
            if (row < 2 && isCardVisible(cardIndex + 3))
                targetIndex = cardIndex + 3;                    // 同列下移
            else if (m_currentPage < getTotalPages() - 1)
            {
                nextPage();                                     // 翻到下一页同列
                targetIndex = col;
            }
            break;
            
        case brls::FocusDirection::UP:
            if (row > 0)
                targetIndex = cardIndex - 3;                    // 同列上移
            else if (m_currentPage > 0)
            {
                prevPage();                                     // 翻到上一页同列
                targetIndex = 6 + col;                          // 上一页第三行同列
            }
            break;
    }
    
    // 目标卡片有效且可见，返回它
    if (targetIndex >= 0 && isCardVisible(targetIndex))
        return m_cards[targetIndex];
    
    // 目标卡片不可见（最后一页不满），向左找最近的可见卡片
    if (targetIndex >= 0)
    {
        for (int i = targetIndex - 1; i >= 0; i--)
        {
            if (isCardVisible(i))
                return m_cards[i];
        }
    }
    
    return nullptr;
}

// 工厂函数：用于 XML 注册
brls::View* GridPage::create()
{
    return new GridPage();
}
