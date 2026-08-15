/**
 * GlobalFooter - 全局常驻底部栏
 */

#include "ui/view/shell/globalFooter.hpp"

GlobalFooter::GlobalFooter() {
    inflateFromXMLRes("xml/view/shell/globalFooter.xml");
}

void GlobalFooter::setIndexText(const std::string& text) {
    m_indexLabel->setText(text);
    m_indexCapsule->setVisibility(text.empty() ? brls::Visibility::GONE : brls::Visibility::VISIBLE);
}

void GlobalFooter::setBackgroundTheme(const std::string& themeKey) {
    setBackgroundColor(themeKey.empty() ? nvgRGBA(0, 0, 0, 0) : brls::Application::getTheme()[themeKey]);
}

brls::View* GlobalFooter::create() {
    return new GlobalFooter();
}
