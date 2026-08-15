/**
 * VersionHistoryCard - 历史版本卡片组件
 * 显示单个历史版本的版本号
 */

#pragma once

#include "ui/view/recyclingGrid.hpp"

class VersionHistoryCard : public RecyclingGridItem {
public:
    /** @brief 创建历史版本卡片并加载 XML 布局 */
    VersionHistoryCard();

    /**
     * @brief 设置卡片版本号
     * @param title 版本号文字
     */
    void setTitle(const std::string& title);

    /** @brief 回收复用时重置内容 */
    void prepareForReuse() override;

    /**
     * @brief 创建可供 RecyclingGrid 使用的历史版本卡片
     * @return 新创建的历史版本卡片
     */
    static RecyclingGridItem* create();

private:
    BRLS_BIND(brls::Label, m_title, "versionHistoryCard/title"); // 历史版本号
};
