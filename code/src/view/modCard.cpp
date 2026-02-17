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
    m_type->setText("类型：" + type);
    m_status->setText(installed ? "模组已安装" : "模组未安装");
    auto theme = brls::Application::getTheme();
    m_status->setTextColor(theme.getColor(installed ? "app/textHighlight" : "app/textSecondary"));
}

void ModCard::prepareForReuse() {
    m_name->setText("");
    m_type->setText("");
    m_status->setText("");
}

RecyclingGridItem* ModCard::create() {
    return new ModCard();
}
