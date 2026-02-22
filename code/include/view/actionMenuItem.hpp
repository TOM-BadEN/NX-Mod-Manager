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

    // 设置选项数据
    void setItem(const std::string& title, const std::string& badge);

    // 设置禁用状态（次级文字颜色）
    void setDisabled(bool disabled);

    // 设置 badge 高亮状态（true=高亮色，false=次级灰色）
    void setBadgeHighlighted(bool highlighted);

    // 回收复用时重置内容
    void prepareForReuse() override;
    void onFocusGained() override;

    // 工厂函数
    static RecyclingGridItem* create();

private:
    BRLS_BIND(brls::Label, m_title, "actionMenuItem/title");  // 标题
    BRLS_BIND(brls::Label, m_badge, "actionMenuItem/badge");  // badge（右侧小字）
};
