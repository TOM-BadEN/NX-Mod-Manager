/**
 * HelpCard - 帮助卡片组件
 * 显示单个帮助条目的标题
 */

#pragma once

#include "ui/view/recyclingGrid.hpp"

class HelpCard : public RecyclingGridItem {
public:
    /** @brief 创建帮助卡片并加载 XML 布局 */
    HelpCard();

    /**
     * @brief 设置卡片标题
     * @param title 标题文字
     */
    void setTitle(const std::string& title);

    /** @brief 回收复用时重置内容 */
    void prepareForReuse() override;

    /**
     * @brief 创建可供 RecyclingGrid 使用的帮助卡片
     * @return 新创建的帮助卡片
     */
    static RecyclingGridItem* create();

private:
    BRLS_BIND(brls::Label, m_title, "helpCard/title"); // 帮助条目标题
};
