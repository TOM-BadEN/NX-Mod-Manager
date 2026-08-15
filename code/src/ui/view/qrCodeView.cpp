/**
 * QrCodeView - 二维码全屏展示组件实现
 */

#include "ui/view/qrCodeView.hpp"
#include "core/audio.hpp"

QrCodeView::QrCodeView(const std::string& text, const std::string& hintText) {
    inflateFromXMLRes("xml/view/qrCodeView.xml");
    m_qrcode->setText(text);
    if (!hintText.empty()) m_hint->setText(hintText);

    auto dismiss = [](brls::View*) {
        Audio::instance()->play(SoundEffect::Enter);
        QrCodeView::close();
        return true;
    };
    registerAction("", brls::BUTTON_B, dismiss, true);
    registerAction("", brls::BUTTON_A, dismiss, true);

    brls::TapGestureConfig tapConfig;
    tapConfig.highlightOnSelect = false;
    addGestureRecognizer(new brls::TapGestureRecognizer(this, tapConfig));
    m_card->addGestureRecognizer(new brls::TapGestureRecognizer(m_card, tapConfig));
}

brls::View* QrCodeView::getDefaultFocus() {
    return m_card;
}

brls::AppletFrame* QrCodeView::getAppletFrame() {
    return nullptr;
}

void QrCodeView::show(const std::string& text, const std::string& hintText) {
    brls::Application::pushActivity(new brls::Activity(new QrCodeView(text, hintText)), brls::TransitionAnimation::NONE);
}

void QrCodeView::close() {
    brls::Application::popActivity();
}
