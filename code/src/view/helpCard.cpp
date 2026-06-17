/**
 * HelpCard - 帮助卡片组件
 * 显示单个帮助条目的标题
 */

#include "view/helpCard.hpp"

HelpCard::HelpCard() {
    inflateFromXMLRes("xml/view/helpCard.xml");
}

void HelpCard::setTitle(const std::string& title) {
    m_title->setText(title);
}

void HelpCard::prepareForReuse() {
    m_title->setText("");
}

RecyclingGridItem* HelpCard::create() {
    return new HelpCard();
}
