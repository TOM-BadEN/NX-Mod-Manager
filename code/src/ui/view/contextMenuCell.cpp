/**
 * ContextMenuCell - 上下文菜单单元格
 */

#include "ui/view/contextMenuCell.hpp"

ContextMenuCell::ContextMenuCell() {
    inflateFromXMLRes("xml/view/contextMenuCell.xml");

    m_switch = new SwitchView();
    m_rightBox->addView(m_switch);

    m_radio = new RadioView();
    m_rightBox->addView(m_radio);

    resetContent();
    applyColors(false);
}

void ContextMenuCell::onFocusGained() {
    RecyclingGridItem::onFocusGained();
    if (m_hasIcon) updateFocusVisuals(true);
}

void ContextMenuCell::onFocusLost() {
    if (m_hasIcon) updateFocusVisuals(false);
    else m_iconScale.reset(1.0f);
    RecyclingGridItem::onFocusLost();
}

void ContextMenuCell::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) {
    if (m_hasIcon) {
        float scale = m_iconScale.getValue();
        float iconX = m_icon->getX();
        float iconY = m_icon->getY();
        float iconWidth = m_icon->getWidth();
        float iconHeight = m_icon->getHeight();
        float centerX = iconX + iconWidth * 0.5f;
        float centerY = iconY + iconHeight * 0.5f;

        nvgSave(vg);
        nvgTranslate(vg, centerX, centerY);
        nvgScale(vg, scale, scale);
        nvgTranslate(vg, -centerX, -centerY);
        m_icon->draw(vg, iconX, iconY, iconWidth, iconHeight, style, ctx);
        nvgRestore(vg);
    }

    RecyclingGridItem::draw(vg, x, y, width, height, style, ctx);
}

void ContextMenuCell::updateFocusVisuals(bool focused) {
    float targetScale = focused ? FOCUS_ICON_SCALE : 1.0f;
    brls::Style style = brls::Application::getStyle();
    m_iconScale.reset();
    m_iconScale.addStep(targetScale, style["brls/animations/highlight"], brls::EasingFunction::quadraticOut);
    m_iconScale.start();
}

void ContextMenuCell::bindBase(const std::string& title, const std::string& icon, bool disabled) {
    bool hadIcon = m_hasIcon;
    resetContent();

    m_title->setText(title);
    if (!icon.empty()) {
        m_hasIcon = true;
        m_icon->setImageFromRes(icon);
        m_icon->setVisibility(brls::Visibility::INVISIBLE);
        if (!hadIcon && isFocused()) updateFocusVisuals(true);
    } else {
        m_iconScale.reset(1.0f);
    }

    m_disabled = disabled;
    m_switch->setEnabled(!disabled);
    m_radio->setEnabled(!disabled);
    applyColors(false);
}

void ContextMenuCell::showBadge(const std::string& badge, bool highlighted) {
    if (badge.empty()) return;

    m_badge->setText(badge);
    m_badge->setVisibility(brls::Visibility::VISIBLE);
    applyColors(highlighted);
}

void ContextMenuCell::showSwitch(bool state, bool animated, bool previousState) {
    if (animated) m_switch->setOn(previousState, false);
    m_switch->setOn(state, animated);
    m_switch->setVisibility(brls::Visibility::VISIBLE);
}

void ContextMenuCell::showRadio(bool selected, bool animated, bool previousState) {
    if (animated) m_radio->setSelected(previousState, false);
    m_radio->setSelected(selected, animated);
    m_radio->setVisibility(brls::Visibility::VISIBLE);
}

void ContextMenuCell::showLoading() {
    m_loading->setVisibility(brls::Visibility::VISIBLE);
}

bool ContextMenuCell::isSwitchAnimating() const {
    return m_switch->isAnimating();
}

void ContextMenuCell::prepareForReuse() {
    m_iconScale.reset(1.0f);
    resetContent();
    applyColors(false);
}

void ContextMenuCell::resetContent() {
    m_disabled = false;
    m_hasIcon = false;

    m_icon->setVisibility(brls::Visibility::GONE);
    m_title->setText("");

    m_badge->setText("");
    m_badge->setVisibility(brls::Visibility::GONE);
    m_loading->setVisibility(brls::Visibility::GONE);

    m_switch->setOn(false, false);
    m_switch->setEnabled(true);
    m_switch->setVisibility(brls::Visibility::GONE);

    m_radio->setSelected(false, false);
    m_radio->setEnabled(true);
    m_radio->setVisibility(brls::Visibility::GONE);
}

void ContextMenuCell::applyColors(bool badgeHighlighted) {
    auto theme = brls::Application::getTheme();
    auto secondary = theme.getColor("app/textSecondary");

    m_title->setTextColor(m_disabled ? secondary : theme.getColor("brls/text"));
    m_badge->setTextColor(
        m_disabled || !badgeHighlighted ? secondary : theme.getColor("app/textHighlight"));
}

RecyclingGridItem* ContextMenuCell::create() {
    return new ContextMenuCell();
}
