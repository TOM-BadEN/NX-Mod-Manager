/**
 * GridPage - 九宫格翻页组件
 * 3x3 布局显示 GameCard，支持 LB/RB 翻页
 */

#pragma once

#include <borealis.hpp>
#include <vector>
#include "view/gameCard.hpp"
#include "utils/indexUpdate.hpp"
#include "common/gameInfo.hpp"

class GridPage : public brls::Box {
public:
    GridPage();
    
    // 自定义导航（上下左右 + 翻页）
    brls::View* getNextFocus(brls::FocusDirection direction, brls::View* currentView) override;
    
    // 设置数据源
    void setGameList(std::vector<GameInfo>& games);
    
    // 翻页
    void nextPage();
    void prevPage();
    
    // 获取当前页码
    int getCurrentPage() const;
    
    // 更新指定全局索引的卡片（如果在当前页则刷新）
    void updateCard(int globalIndex);
    
    // 索引更新（通知外部更新 Footer 索引）
    void setIndexChangeCallback(std::function<void(int, int)> callback);
    
    // 工厂函数
    static brls::View* create();

private:
    static constexpr int CARDS_PER_PAGE = 9;               // 每页卡片数
    
    GameCard* m_cards[CARDS_PER_PAGE];                     // 固定 9 张卡片
    std::vector<GameInfo>* m_games = nullptr;               // 指向外部数据（不拥有）
    int m_currentPage = 0;                                 // 当前页码（从 0 开始）
    IndexUpdate m_indexUpdate;                              // 索引更新工具
    
    int getTotalPages();                                   // 总页数
    void refreshPage();                                    // 刷新卡片数据（不处理焦点）
    void flipPage(int delta);                              // 纯翻页：改页码 + 刷新数据
    void focusCard(int cardIndex);                         // 焦点转移 + 索引更新
    void fixFocusAfterFlip();                              // 翻页后修正不可见焦点
    int findFocusedCardIndex();                            // 查找当前聚焦的卡片索引
    bool isCardVisible(int index);                         // 卡片是否可见
    int findVisibleCard(int start, int step);              // 从 start 按 step 方向找最近可见卡片
};
