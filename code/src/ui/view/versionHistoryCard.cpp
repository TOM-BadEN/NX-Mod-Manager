/**
 * VersionHistoryCard - 历史版本卡片组件
 * 显示单个历史版本的版本号
 */

#include "ui/view/versionHistoryCard.hpp"

VersionHistoryCard::VersionHistoryCard() {
    inflateFromXMLRes("xml/view/versionHistoryCard.xml");
}

void VersionHistoryCard::setTitle(const std::string& title) {
    m_title->setText(title);
}

void VersionHistoryCard::prepareForReuse() {
    m_title->setText("");
}

RecyclingGridItem* VersionHistoryCard::create() {
    return new VersionHistoryCard();
}
