/**
 * QrCodeView - 二维码全屏展示组件实现
 */

#include "view/qrCodeView.hpp"
#include <borealis/core/i18n.hpp>
#include "core/audio.hpp"
#include "utils/pageNav.hpp"

QrCodeView::QrCodeView(const std::string& text)
{
    inflateFromXMLRes("xml/view/qrCodeView.xml");
    m_qrcode->setText(text);

    auto dismiss = [](brls::View*) {
        Audio::instance()->play(SoundEffect::Enter);
        QrCodeView::close();
        return true;
    };
    registerAction(brls::getStr("views/qrCodeView/back"), brls::BUTTON_B, dismiss);
    registerAction(brls::getStr("views/qrCodeView/confirm"), brls::BUTTON_A, dismiss);

    brls::TapGestureConfig tapConfig;
    tapConfig.highlightOnSelect = false;
    addGestureRecognizer(new brls::TapGestureRecognizer(this, tapConfig));
    m_card->addGestureRecognizer(new brls::TapGestureRecognizer(m_card, tapConfig));
}

brls::View* QrCodeView::getDefaultFocus()
{
    return m_card;
}

brls::AppletFrame* QrCodeView::getAppletFrame()
{
    return nullptr;
}

void QrCodeView::show(const std::string& text)
{
    pageNav::push(new brls::Activity(new QrCodeView(text)));
}

void QrCodeView::close()
{
    brls::Application::popActivity();
}
