/**
 * GlobalFooter - 全局常驻底部栏
 *
 * 只负责索引文字和 Borealis 按键提示的显示，不持有 Page、
 * PageHost 或 AppShell 指针。
 */

#pragma once

#include "ui/view/capsuleBadge.hpp"
#include <borealis.hpp>
#include <string>

class GlobalFooter : public brls::Box {
public:
    GlobalFooter();

    /**
     * @brief 设置底部栏索引文字
     * @param text 索引文字，空字符串表示不显示
     */
    void setIndexText(const std::string& text);

    /**
     * @brief 设置底部栏背景主题色
     * @param themeKey 主题色名称，空字符串表示透明
     */
    void setBackgroundTheme(const std::string& themeKey);

    /** @brief XML View 工厂函数 */
    static brls::View* create();

private:
    // XML 绑定的组件
    BRLS_BIND(CapsuleBadge, m_indexCapsule, "globalFooter/indexCapsule");
    BRLS_BIND(brls::Label, m_indexLabel, "globalFooter/index");
};
