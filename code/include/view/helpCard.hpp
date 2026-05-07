/**
 * HelpCard - 帮助卡片组件
 * 显示单个帮助条目的标题
 */

#pragma once

#include "view/recyclingGrid.hpp"

class HelpCard : public RecyclingGridItem {
public:
    HelpCard();

    /**
     * @brief 设置卡片标题
     * @param title 标题文字
     */
    void setTitle(const std::string& title);

    /** @brief 回收复用时重置内容 */
    void prepareForReuse() override;

    /** @brief 工厂函数 */
    static RecyclingGridItem* create();

private:
    BRLS_BIND(brls::Label, m_title, "helpCard/title");
};
