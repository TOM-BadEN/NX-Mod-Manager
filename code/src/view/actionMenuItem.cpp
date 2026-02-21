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

void ActionMenuItem::prepareForReuse() {
    YGNodeStyleSetDisplay(m_badge->getYGNode(), YGDisplayNone);
    setDisabled(false);
}

RecyclingGridItem* ActionMenuItem::create() {
    return new ActionMenuItem();
}
