/**
 * GridPage - 通用滚动网格组件
 * 继承 ScrollingFrame（CENTERED），支持触屏滑动 + 方向键滚动 + LB/RB 快速翻屏
 * 导航采用 index±cols 数学保列，零业务依赖，数据绑定通过回调实现
 */

#pragma once

#include <borealis.hpp>
#include <vector>
#include <functional>
#include "utils/indexUpdate.hpp"

class GridPage;

// 内部容器：重写焦点导航，委托给 GridPage
class GridContentBox : public brls::Box {
public:
    GridContentBox(GridPage* grid);
    brls::View* getNextFocus(brls::FocusDirection direction, brls::View* currentView) override;
private:
    GridPage* m_grid;
};

class GridPage : public brls::ScrollingFrame {
    friend class GridContentBox;
public:
    GridPage();

    // 初始化网格：设置列数 + 工厂函数（行数由数据量决定）
    void setGrid(int cols, std::function<brls::View*()> factory);

    // 设置数据（合并 setItemCount + setBindCallback + rebuild + bindAll）
    void setData(int count, std::function<void(brls::View*, int)> bind);

    // 设置数据总数（数量变化时重建）
    void setItemCount(int count);

    // 数据变更后重新绑定（不重建 View）
    void reloadData();

    // 更新指定全局索引的槽位
    void updateSlot(int globalIndex);

    // 元素点击回调（传出全局索引）
    void setClickCallback(std::function<void(int)> callback);

    // 索引更新回调
    void setIndexChangeCallback(std::function<void(int, int)> callback);

    static brls::View* create();

private:
    int m_cols = 0;                                             // 网格列数
    int m_totalItems = 0;                                       // 数据总数
    GridContentBox* m_contentBox = nullptr;                     // 内部容器

    std::vector<brls::View*> m_items;                           // 全部数据元素（不含占位）
    IndexUpdate m_indexUpdate;                                  // 索引更新工具
    std::function<brls::View*()> m_factory;                     // 元素工厂函数
    std::function<void(brls::View*, int)> m_bindCallback;       // 数据绑定回调
    std::function<void(int)> m_clickCallback;                   // 点击回调

    brls::View* getNextCellFocus(brls::FocusDirection direction, brls::View* currentView);
    void flipScreen(int direction);                             // LB/RB 翻屏
    void rebuild();                                             // 按 totalItems 重建全部行和元素
    void bindAll();                                             // 绑定全部元素数据
    int findFocusedIndex();                                     // 查找当前聚焦元素的全局索引
};
