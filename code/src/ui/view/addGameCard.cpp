/**
 * AddGameCard - 新增游戏卡片组件
 * 显示单个游戏的图标、名称、版本和 MOD 数量（黑绿配色）
 */

#include "ui/view/addGameCard.hpp"
#include "utils/format.hpp"
#include <borealis/core/cache_helper.hpp>
#include <borealis/core/i18n.hpp>

AddGameCard::AddGameCard() {
    inflateFromXMLRes("xml/view/addGameCard.xml");
    m_icon->setFreeTexture(false);
    m_defaultIconId = m_icon->getTexture();
    m_icon->setVisibility(brls::Visibility::INVISIBLE);
}

AddGameCard::~AddGameCard() {
    resetIcon();
}

void AddGameCard::onFocusGained() {
    RecyclingGridItem::onFocusGained();
    updateFocusVisuals(true);
}

void AddGameCard::onFocusLost() {
    updateFocusVisuals(false);
    RecyclingGridItem::onFocusLost();
}

void AddGameCard::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) {
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
    m_mask->draw(vg, m_mask->getX(), m_mask->getY(), m_mask->getWidth(), m_mask->getHeight(), style, ctx);
    nvgRestore(vg);

    RecyclingGridItem::draw(vg, x, y, width, height, style, ctx);
}

void AddGameCard::updateFocusVisuals(bool focused) {
    float targetScale = focused ? FOCUS_ICON_SCALE : 1.0f;
    brls::Style style = brls::Application::getStyle();
    m_iconScale.reset();
    m_iconScale.addStep(targetScale, style["brls/animations/highlight"], brls::EasingFunction::quadraticOut);
    m_iconScale.start();
}

void AddGameCard::setGame(const std::string& name, const std::string& version, const std::string& modCount) {
    m_name->setText(name.empty() ? brls::getStr("view/addGameCard/virtualGame") : name);
    m_version->setText(format::cleanVersion(version));
    m_modCount->setText(modCount.empty() || modCount == "0" ? "0" : modCount);
}

void AddGameCard::setIcon(int iconId) {
    if (iconId <= 0) return;
    m_iconId = iconId;
    m_icon->innerSetImage(iconId);
}

void AddGameCard::resetIcon() {
    if (m_iconId <= 0) return;
    brls::TextureCache::instance().removeCache(m_iconId);
    m_iconId = 0;
    m_icon->innerSetImage(m_defaultIconId);
}

void AddGameCard::prepareForReuse() {
    m_iconScale.reset(1.0f);
}

void AddGameCard::cacheForReuse() {
    resetIcon();
}

RecyclingGridItem* AddGameCard::create() {
    return new AddGameCard();
}
