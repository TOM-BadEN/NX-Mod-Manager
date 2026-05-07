/**
 * ModCard - Mod 卡片组件
 * 显示单个 mod 的图标、名称、类型和安装状态
 */

#include "view/modCard.hpp"
#include <borealis/core/i18n.hpp>
#include "common/modInfo.hpp"

ModCard::ModCard() {
    inflateFromXMLRes("xml/view/modCard.xml");
}

void ModCard::setMod(const std::string& name, const std::string& type, bool installed, bool disabled, int modID) {
    m_icon->setImageFromRes(modTypeIcon(type));
    m_name->setText(name);
    m_type->setText(brls::getStr("views/modCard/type", modTypeText(type)));
    auto theme = brls::Application::getTheme();
    if (disabled) {
        m_status->setText(brls::getStr("views/modCard/disabled"));
        m_status->setTextColor(theme.getColor("app/textWarning"));
    } else {
        m_status->setText(installed ? brls::getStr("views/modCard/installed") : brls::getStr("views/modCard/notInstalled"));
        m_status->setTextColor(theme.getColor(installed ? "app/textHighlight" : "app/textSecondary"));
    }
    m_cloudIcon->setVisibility(modID >= 0 ? brls::Visibility::VISIBLE : brls::Visibility::INVISIBLE);
}

void ModCard::prepareForReuse() {
    m_name->setText("");
    m_type->setText("");
    m_status->setText("");
    m_cloudIcon->setVisibility(brls::Visibility::INVISIBLE);
}

RecyclingGridItem* ModCard::create() {
    return new ModCard();
}
