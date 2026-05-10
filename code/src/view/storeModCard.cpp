/**
 * StoreModCard - 商店模组卡片组件
 * 显示单个模组的图标、名称、标签（版本/作者/类型/适配版本）和统计数据
 */

#include "view/storeModCard.hpp"
#include <borealis/core/i18n.hpp>
#include "common/modInfo.hpp"
#include "utils/format.hpp"

StoreModCard::StoreModCard() {
    inflateFromXMLRes("xml/view/storeModCard.xml");

    // 根据主题选择图标颜色：dark 主题用白色，light 主题用深色
    bool isDarkTheme = brls::Application::getThemeVariant() == brls::ThemeVariant::DARK;
    std::string suffix = isDarkTheme ? "-dark.svg" : "-light.svg";
    m_iconLike->setImageFromSVGRes("img/mod/like" + suffix);
    m_iconDislike->setImageFromSVGRes("img/mod/dislike" + suffix);
    m_iconDownload->setImageFromSVGRes("img/mod/download" + suffix);
    m_iconTime->setImageFromSVGRes("img/mod/time" + suffix);
}

void StoreModCard::setMod(const std::string& name,
                          const std::string& uploadTime,
                          const std::string& type,
                          const std::string& gameVersion,
                          int likes, int dislikes, int downloads,
                          bool isInstalled) {
    m_icon->setImageFromRes(modTypeIcon(type));
    m_name->setText(name);
    m_installedIcon->setVisibility(isInstalled ? brls::Visibility::VISIBLE : brls::Visibility::INVISIBLE);
    m_tagType->setText(modTypeText(type));
    if (gameVersion.empty()) m_tagGameVersion->setText(brls::getStr("views/storeModCard/gameVersionUnknown"));
    else if (gameVersion == "0") m_tagGameVersion->setText(brls::getStr("views/storeModCard/gameVersionUniversal"));
    else m_tagGameVersion->setText(brls::getStr("views/storeModCard/gameVersionFmt", gameVersion));
    m_statLike->setText(std::to_string(likes));
    m_statDislike->setText(std::to_string(dislikes));
    m_statDownload->setText(std::to_string(downloads));
    m_statTime->setText(format::timeAgo(uploadTime));
}

void StoreModCard::prepareForReuse() {
    m_name->setText("");
    m_tagType->setText("");
    m_tagGameVersion->setText("");
    m_statLike->setText("");
    m_statDislike->setText("");
    m_statDownload->setText("");
    m_statTime->setText("");
    m_installedIcon->setVisibility(brls::Visibility::INVISIBLE);
}

RecyclingGridItem* StoreModCard::create() {
    return new StoreModCard();
}
