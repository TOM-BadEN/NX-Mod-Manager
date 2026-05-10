/**
 * ActionMenuItem - 菜单选项组件
 * 显示单个菜单项的标题和 badge
 */

#include "view/actionMenuItem.hpp"
#include <yoga/Yoga.h>

ActionMenuItem::ActionMenuItem() {
    inflateFromXMLRes("xml/view/actionMenuItem.xml");
}

void ActionMenuItem::setItem(const std::string& title, const std::string& badge) {
    m_title->setText(title);
    if (badge.empty()) {
        // 空 badge 从 Yoga 布局中排除，避免 Borealis labelMeasureFunc 空文本缓存 bug
        YGNodeStyleSetDisplay(m_badge->getYGNode(), YGDisplayNone);
        m_badge->invalidate();
    } else {
        YGNodeStyleSetDisplay(m_badge->getYGNode(), YGDisplayFlex);
        m_badge->setText(badge);
    }
}

void ActionMenuItem::setDisabled(bool disabled) {
    auto theme = brls::Application::getTheme();
    auto color = theme.getColor(disabled ? "app/textSecondary" : "brls/text");
    m_title->setTextColor(color);
    m_badge->setTextColor(disabled ? color : theme.getColor("app/textHighlight"));
}

void ActionMenuItem::setBadgeHighlighted(bool highlighted) {
    auto theme = brls::Application::getTheme();
    m_badge->setTextColor(theme.getColor(highlighted ? "app/textHighlight" : "app/textSecondary"));
}

void ActionMenuItem::setMultiSelectLayout(bool multiSelect) {
    if (multiSelect) {
        YGNodeStyleSetFlexShrink(m_title->getYGNode(), 1);
        YGNodeStyleSetFlexShrink(m_badge->getYGNode(), 0);
    } else {
        YGNodeStyleSetFlexShrink(m_title->getYGNode(), 0);
        YGNodeStyleSetFlexShrink(m_badge->getYGNode(), 1);
    }
}

void ActionMenuItem::prepareForReuse() {
    YGNodeStyleSetDisplay(m_badge->getYGNode(), YGDisplayNone);
    YGNodeStyleSetFlexShrink(m_title->getYGNode(), 0);
    YGNodeStyleSetFlexShrink(m_badge->getYGNode(), 1);
    setDisabled(false);
}

RecyclingGridItem* ActionMenuItem::create() {
    return new ActionMenuItem();
}
