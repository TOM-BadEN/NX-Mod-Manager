/**
 * AddGameCard - 新增游戏卡片组件
 * 显示单个游戏的图标、名称、版本和 mod 数量（黑绿配色）
 */

#include "view/addGameCard.hpp"
#include <borealis/core/i18n.hpp>
#include "utils/format.hpp"

// 构造函数：加载 XML 布局
AddGameCard::AddGameCard() {
    inflateFromXMLRes("xml/view/addGameCard.xml");
    m_icon->setFreeTexture(false);
    m_defaultIconId = m_icon->getTexture();
}

// 设置卡片数据
void AddGameCard::setGame(const std::string& name, const std::string& version, const std::string& modCount) {
    m_name->setText(name.empty() ? brls::getStr("views/gameCard/virtualGame") : name);
    m_version->setText(format::cleanVersion(version));
    m_modCount->setText(modCount.empty() || modCount == "0" ? "0" : modCount);
}

// 设置游戏图标（传入 NVG 纹理 ID）
void AddGameCard::setIcon(int iconId) {
    if (iconId <= 0) return;
    m_icon->innerSetImage(iconId);
}

void AddGameCard::resetIcon() {
    m_icon->innerSetImage(m_defaultIconId);
}

void AddGameCard::prepareForReuse() {
    resetIcon();
    m_like->setVisibility(brls::Visibility::GONE);
    m_name->setText("");
    m_version->setText("");
    m_modCount->setText("");
}

RecyclingGridItem* AddGameCard::create() {
    return new AddGameCard();
}
