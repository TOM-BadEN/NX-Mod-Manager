/**
 * StoreGameCard - 商店游戏卡片组件
 * 显示单个游戏的图标、名称、MOD数量和更新时间
 */

#include "ui/view/storeGameCard.hpp"
#include "utils/format.hpp"
#include <borealis/core/cache_helper.hpp>

StoreGameCard::StoreGameCard() {
    inflateFromXMLRes("xml/view/storeGameCard.xml");
    m_icon->setFreeTexture(false);
    m_defaultIconId = m_icon->getTexture();
    m_icon->setVisibility(brls::Visibility::INVISIBLE);
}

StoreGameCard::~StoreGameCard() {
    resetIcon();
}

void StoreGameCard::onFocusGained() {
    RecyclingGridItem::onFocusGained();
    updateFocusVisuals(true);
}

void StoreGameCard::onFocusLost() {
    updateFocusVisuals(false);
    RecyclingGridItem::onFocusLost();
}

void StoreGameCard::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) {
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

void StoreGameCard::updateFocusVisuals(bool focused) {
    float targetScale = focused ? FOCUS_ICON_SCALE : 1.0f;
    brls::Style style = brls::Application::getStyle();
    m_iconScale.reset();
    m_iconScale.addStep(targetScale, style["brls/animations/highlight"], brls::EasingFunction::quadraticOut);
    m_iconScale.start();
}

void StoreGameCard::setGame(const std::string& name, const std::string& modCount, const std::string& lastUpdate) {
    m_name->setText(name);
    m_modCount->setText(modCount);
    m_lastUpdate->setText(format::timeAgo(lastUpdate));
}

void StoreGameCard::setIcon(int iconId) {
    if (iconId <= 0) return;
    m_iconId = iconId;
    m_icon->innerSetImage(iconId);
}

void StoreGameCard::setInstalled(bool installed) {
    m_installed->setVisibility(installed ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
}

void StoreGameCard::resetIcon() {
    if (m_iconId <= 0) return;
    brls::TextureCache::instance().removeCache(m_iconId);
    m_iconId = 0;
    m_icon->innerSetImage(m_defaultIconId);
}

void StoreGameCard::prepareForReuse() {
    m_iconScale.reset(1.0f);
}

void StoreGameCard::cacheForReuse() {
    resetIcon();
}

RecyclingGridItem* StoreGameCard::create() {
    return new StoreGameCard();
}
