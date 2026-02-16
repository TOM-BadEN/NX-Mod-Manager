/**
 * ModCard - Mod 卡片组件
 * 显示单个 mod 的图标、名称、类型和安装状态
 */

#include "view/modCard.hpp"

ModCard::ModCard() {
    inflateFromXMLRes("xml/view/modCard.xml");
}

void ModCard::setMod(const std::string& name, const std::string& type, bool installed) {
    m_icon->setImageFromRes("img/defaultIcon.jpg");
    m_name->setText(name);
    m_type->setText(type);
    m_status->setText(installed ? "已安装" : "未安装");
    setVisibility(brls::Visibility::VISIBLE);
    setFocusable(true);
}

void ModCard::clear() {
    m_name->setText("");
    m_type->setText("");
    m_status->setText("");
    setVisibility(brls::Visibility::INVISIBLE);
    setFocusable(false);
}

brls::View* ModCard::create() {
    return new ModCard();
}
