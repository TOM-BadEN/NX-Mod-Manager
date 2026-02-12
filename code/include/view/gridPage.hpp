/**
 * GridPage - 九宫格翻页组件
 * 3x3 布局显示 GameCard，支持 LB/RB 翻页
 */

#pragma once

#include <borealis.hpp>
#include <vector>
#include "view/gameCard.hpp"
#include "utils/indexUpdate.hpp"

// 游戏数据
struct GameInfo {
    std::string name;
    std::string version;
    std::string modCount;
    int iconId = 0;
};

class GridPage : public brls::Box {
public:
    GridPage();
    
    // 自定义导航（上下左右 + 翻页）
    brls::View* getNextFocus(brls::FocusDirection direction, brls::View* currentView) override;
    
    // 设置数据源
    void setGameList(const std::vector<GameInfo>& games);
    
    // 翻页
    void nextPage();
    void prevPage();
    
    // 索引更新（通知外部更新 Footer 索引）
    void setIndexChangeCallback(std::function<void(int, int)> callback);
    
    // 工厂函数
    static brls::View* create();

private:
    static constexpr int CARDS_PER_PAGE = 9;               // 每页卡片数
    
    GameCard* m_cards[CARDS_PER_PAGE];                     // 固定 9 张卡片
    std::vector<GameInfo> m_games;                         // 全部游戏数据
    int m_currentPage = 0;                                 // 当前页码（从 0 开始）
    IndexUpdate m_indexUpdate;                              // 索引更新工具
    
    int getTotalPages();                                   // 总页数（内部翻页用）
    void refreshPage();                                    // 刷新当前页的卡片内容
    int findFocusedCardIndex();                            // 查找当前聚焦的卡片索引
    bool isCardVisible(int index);                         // 卡片是否可见
};
