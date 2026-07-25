/**
 * GlobalFooter - 全局常驻底部栏
 */

#include "ui/view/shell/globalFooter.hpp"

GlobalFooter::GlobalFooter() {
    inflateFromXMLRes("xml/view/shell/globalFooter.xml");
}

void GlobalFooter::setIndexText(const std::string& text) {
    m_indexLabel->setText(text);
    m_indexLabel->setVisibility(text.empty() ? brls::Visibility::GONE : brls::Visibility::VISIBLE);
}

brls::View* GlobalFooter::create() {
    return new GlobalFooter();
}
