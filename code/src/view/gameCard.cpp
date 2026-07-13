/**
 * GameCard - 游戏卡片组件
 * 显示单个游戏的图标、名称、版本和 mod 数量
 */

#include "view/gameCard.hpp"
#include <borealis/core/i18n.hpp>
#include "utils/format.hpp"

// 构造函数：加载 XML 布局
GameCard::GameCard() {
    inflateFromXMLRes("xml/view/gameCard.xml");
    m_icon->setFreeTexture(false);
    m_defaultIconId = m_icon->getTexture();
}

// 设置卡片数据
void GameCard::setGame(const std::string& name, const std::string& version, const std::string& modCount) {
    m_name->setText(name.empty() ? brls::getStr("views/gameCard/virtualGame") : name);
    m_version->setText(format::cleanVersion(version));
    m_modCount->setText(modCount.empty() || modCount == "0" ? "0" : modCount);
}

// 设置游戏图标（传入 NVG 纹理 ID）
void GameCard::setIcon(int iconId) {
    if (iconId <= 0) return;
    m_icon->innerSetImage(iconId);
}                                 

void GameCard::setFavorite(bool favorite) {
    m_like->setVisibility(favorite ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
}

void GameCard::setNameColor(NVGcolor color) {
    m_name->setTextColor(color);
}

void GameCard::resetIcon() {
    m_icon->innerSetImage(m_defaultIconId);
}

void GameCard::prepareForReuse() {
    resetIcon();
    m_like->setVisibility(brls::Visibility::GONE);
    m_name->setText("");
    m_name->setTextColor(brls::Application::getTheme()["app/cardOverlayText"]);
    m_version->setText("");
    m_modCount->setText("");
}

RecyclingGridItem* GameCard::create() {
    return new GameCard();
}
