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

// 工厂函数：用于 XML 注册
brls::View* GridPage::create()
{
    return new GridPage();
}
