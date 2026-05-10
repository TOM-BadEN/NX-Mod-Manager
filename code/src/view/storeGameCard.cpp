/**
 * StoreGameCard - 商店游戏卡片组件
 * 显示单个游戏的图标、名称、MOD数量和更新时间
 */

#include "view/storeGameCard.hpp"
#include <borealis/core/i18n.hpp>
#include "utils/format.hpp"

StoreGameCard::StoreGameCard() {
    inflateFromXMLRes("xml/view/storeGameCard.xml");
    m_icon->setFreeTexture(false);
    m_defaultIconId = m_icon->getTexture();
}

void StoreGameCard::setGame(const std::string& name, const std::string& modCount, const std::string& lastUpdate) {
    m_name->setText(name);
    m_modCount->setText(brls::getStr("views/storeGameCard/count", modCount));
    m_lastUpdate->setText(brls::getStr("views/storeGameCard/date", format::timeAgo(lastUpdate)));
}

void StoreGameCard::setIcon(int iconId) {
    if (iconId <= 0) return;
    m_icon->innerSetImage(iconId);
}

void StoreGameCard::setInstalled(bool installed) {
    m_installed->setVisibility(installed ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
}

void StoreGameCard::prepareForReuse() {
    m_icon->innerSetImage(m_defaultIconId);
    m_installed->setVisibility(brls::Visibility::GONE);
    m_name->setText("");
    m_modCount->setText("");
    m_lastUpdate->setText("");
}

RecyclingGridItem* StoreGameCard::create() {
    return new StoreGameCard();
}
