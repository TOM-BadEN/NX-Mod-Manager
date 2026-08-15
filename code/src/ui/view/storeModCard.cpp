/**
 * StoreModCard - 商店模组卡片组件
 * 显示单个模组的图标、名称、标签（版本/作者/类型/适配版本）和统计数据
 */

#include "ui/view/storeModCard.hpp"
#include "common/modInfo.hpp"
#include "utils/format.hpp"
#include <borealis/core/i18n.hpp>

StoreModCard::StoreModCard() {
    inflateFromXMLRes("xml/view/storeModCard.xml");
    m_icon->setVisibility(brls::Visibility::INVISIBLE);

    m_iconLike->setImageFromRes(format::themedIconPath("img/mod/like"));
    m_iconDislike->setImageFromRes(format::themedIconPath("img/mod/dislike"));
    m_iconDownload->setImageFromRes(format::themedIconPath("img/mod/download"));
    m_iconTime->setImageFromRes(format::themedIconPath("img/mod/time"));
}

void StoreModCard::onFocusGained() {
    RecyclingGridItem::onFocusGained();
    updateFocusVisuals(true);
}

void StoreModCard::onFocusLost() {
    updateFocusVisuals(false);
    RecyclingGridItem::onFocusLost();
}

void StoreModCard::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) {
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

void StoreModCard::updateFocusVisuals(bool focused) {
    float targetScale = focused ? FOCUS_ICON_SCALE : 1.0f;
    brls::Style style = brls::Application::getStyle();
    m_iconScale.reset();
    m_iconScale.addStep(targetScale, style["brls/animations/highlight"], brls::EasingFunction::quadraticOut);
    m_iconScale.start();
}

void StoreModCard::setMod(const std::string& name, const std::string& uploadTime, const std::string& type, const std::string& gameVersion, int likes, int dislikes, int downloads, bool downloaded, bool hasUpdate) {
    m_icon->setImageFromRes(modTypeIcon(type));
    m_name->setText(name);
    m_installedIcon->setVisibility(downloaded ? brls::Visibility::VISIBLE : brls::Visibility::INVISIBLE);
    m_updateBadge->setVisibility(downloaded && hasUpdate ? brls::Visibility::VISIBLE : brls::Visibility::INVISIBLE);
    m_tagType->setText(modTypeText(type));
    if (gameVersion.empty()) m_tagGameVersion->setText(brls::getStr("view/storeModCard/gameVersionUnknown"));
    else if (gameVersion == "0") m_tagGameVersion->setText(brls::getStr("view/storeModCard/gameVersionUniversal"));
    else m_tagGameVersion->setText(brls::getStr("view/storeModCard/gameVersionFmt", format::cleanVersion(gameVersion)));
    m_statLike->setText(std::to_string(likes));
    m_statDislike->setText(std::to_string(dislikes));
    m_statDownload->setText(std::to_string(downloads));
    m_statTime->setText(format::timeAgo(uploadTime));
}

void StoreModCard::prepareForReuse() {
    m_iconScale.reset(1.0f);
}

RecyclingGridItem* StoreModCard::create() {
    return new StoreModCard();
}
