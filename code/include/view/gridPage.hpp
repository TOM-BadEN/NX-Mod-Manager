/**
 * GridPage - 通用网格翻页组件
 * 参数化行列数 + 工厂函数动态创建元素，支持 LB/RB 翻页
 * 零业务依赖，数据绑定通过回调实现
 */

#pragma once

#include <borealis.hpp>
#include <vector>
#include <functional>
#include "utils/indexUpdate.hpp"

class GridPage : public brls::Box {
public:
    GridPage();

    // 初始化网格：创建 rows×cols 个元素
    void setGrid(int rows, int cols, std::function<brls::View*()> factory);

    // 设置数据（合并 setItemCount + setBindCallback + reloadData）
    void setData(int count, std::function<void(brls::View*, int)> bind);

    // 设置数据总数
    void setItemCount(int count);

    // 数据变更后重刷当前页（不重置页码）
    void reloadData();

    // 元素点击回调（传出全局索引）
    void setClickCallback(std::function<void(int)> callback);

    // 索引更新回调
    void setIndexChangeCallback(std::function<void(int, int)> callback);

    // 翻页
    void nextPage();
    void prevPage();
    int getCurrentPage() const;

    // 更新指定全局索引的槽位（如果在当前页则调用 refreshCallback）
    void updateSlot(int globalIndex);

    // 焦点导航
    brls::View* getNextFocus(brls::FocusDirection direction, brls::View* currentView) override;

    static brls::View* create();

private:
    int m_rows = 0;                                             // 网格行数
    int m_cols = 0;                                             // 网格列数
    int m_itemsPerPage = 0;                                     // 每页元素数（rows × cols）
    int m_totalItems = 0;                                       // 数据总数
    int m_currentPage = 0;                                      // 当前页码（从 0 开始）

    std::vector<brls::View*> m_items;                           // 元素槽位（通用 View*）
    IndexUpdate m_indexUpdate;                                  // 索引更新工具

    std::function<void(brls::View*, int)> m_bindCallback;       // 数据绑定回调（槽位, 全局索引）
    std::function<void(int)> m_clickCallback;                   // 点击回调（全局索引）

    int getTotalPages();                                        // 计算总页数
    void refreshPage();                                         // 刷新当前页所有元素
    void flipPage(int delta);                                   // 翻页（+1/-1）+ 刷新数据
    void focusItem(int itemIndex);                              // 聚焦指定元素 + 更新索引
    void fixFocusAfterFlip();                                   // 翻页后修正不可见焦点
    int findFocusedIndex();                                     // 查找当前聚焦的元素索引
    bool isItemVisible(int index);                              // 元素是否可见
    int findVisibleItem(int start, int step);                   // 按方向查找最近可见元素
};
