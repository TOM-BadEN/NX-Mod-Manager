/**
 * ModCard - Mod 卡片组件
 * 显示单个 mod 的图标、名称、类型和安装状态
 */

#include "ui/view/modCard.hpp"
#include <borealis/core/i18n.hpp>
#include "common/modInfo.hpp"

ModCard::ModCard() {
    inflateFromXMLRes("xml/view/modCard.xml");
    m_icon->setVisibility(brls::Visibility::INVISIBLE);
}

void ModCard::onFocusGained() {
    RecyclingGridItem::onFocusGained();
    updateFocusVisuals(true);
}

void ModCard::onFocusLost() {
    updateFocusVisuals(false);
    RecyclingGridItem::onFocusLost();
}

void ModCard::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) {
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

    RecyclingGridItem::draw(vg, x, y, width, height, style, ctx);
}

void ModCard::updateFocusVisuals(bool focused) {
    float targetScale = focused ? FOCUS_ICON_SCALE : 1.0f;
    brls::Style style = brls::Application::getStyle();
    m_iconScale.reset();
    m_iconScale.addStep(targetScale, style["brls/animations/highlight"], brls::EasingFunction::quadraticOut);
    m_iconScale.start();
}

void ModCard::setMod(const std::string& name, const std::string& type, bool installed, bool disabled, int modID, bool hasUpdate) {
    m_icon->setImageFromRes(modTypeIcon(type));
    m_name->setText(name);
    m_type->setText(brls::getStr("view/modCard/type", modTypeText(type)));
    auto theme = brls::Application::getTheme();
    if (disabled) {
        m_status->setText(brls::getStr("view/modCard/disabled"));
        m_status->setTextColor(theme.getColor("app/textWarning"));
    } else {
        m_status->setText(installed ? brls::getStr("view/modCard/installed") : brls::getStr("view/modCard/notInstalled"));
        m_status->setTextColor(theme.getColor(installed ? "app/textHighlight" : "app/textSecondary"));
    }
    bool isStoreMod = modID >= 0;
    m_cloudIcon->setVisibility(isStoreMod ? brls::Visibility::VISIBLE : brls::Visibility::INVISIBLE);
    m_updateBadge->setVisibility(hasUpdate && isStoreMod ? brls::Visibility::VISIBLE : brls::Visibility::INVISIBLE);
}

void ModCard::prepareForReuse() {
    m_iconScale.reset(1.0f);
    m_name->setText("");
    m_type->setText("");
    m_status->setText("");
    m_cloudIcon->setVisibility(brls::Visibility::INVISIBLE);
    m_updateBadge->setVisibility(brls::Visibility::INVISIBLE);
}

RecyclingGridItem* ModCard::create() {
    return new ModCard();
}
