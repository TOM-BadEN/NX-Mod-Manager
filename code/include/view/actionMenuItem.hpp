/**
 * ActionMenuItem - 菜单选项组件
 * 显示单个菜单项的标题和 badge
 */

#pragma once

#include <borealis.hpp>
#include "view/recyclingGrid.hpp"

class ActionMenuItem : public RecyclingGridItem {
public:
    ActionMenuItem();

    /**
     * @brief 设置选项数据
     * @param title 标题
     * @param badge 右侧小字
     */
    void setItem(const std::string& title, const std::string& badge);

    /**
     * @brief 设置禁用状态
     * @param disabled 是否禁用
     */
    void setDisabled(bool disabled);

    /**
     * @brief 设置 badge 高亮状态
     * @param highlighted 是否高亮
     */
    void setBadgeHighlighted(bool highlighted);

    /**
     * @brief 多选模式布局（标题可缩，勾不可缩）
     * @param multiSelect 是否多选模式
     */
    void setMultiSelectLayout(bool multiSelect);

    /** @brief 回收复用时重置内容 */
    void prepareForReuse() override;

    /** @brief 工厂函数 */
    static RecyclingGridItem* create();

private:
    BRLS_BIND(brls::Label, m_title, "actionMenuItem/title");  // 标题
    BRLS_BIND(brls::Label, m_badge, "actionMenuItem/badge");  // badge（右侧小字）
};
