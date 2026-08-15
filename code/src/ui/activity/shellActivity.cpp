/**
 * ShellActivity - 全局 UI Shell 的常驻 Activity
 */

#include "ui/activity/shellActivity.hpp"
#include "core/audio.hpp"
#include "ui/view/dialog/customDialog.hpp"
#include <borealis/core/i18n.hpp>

ShellActivity::ShellActivity(Page* rootPage)
    : m_rootPage(rootPage) {
    if (!m_rootPage) brls::fatal("ShellActivity requires a root Page");
}

ShellActivity::~ShellActivity() = default;

brls::View* ShellActivity::createContentView() {
    m_shell = new AppShell();
    return m_shell;
}

void ShellActivity::onContentAvailable() {
    if (!m_shell->getPageHost()->setRootPage(m_rootPage.get())) {
        brls::fatal("ShellActivity failed to attach the root Page");
    }
    m_rootPage.release();

    registerAction("", brls::BUTTON_B, [this](brls::View*) {
        return handleBack();
    }, true);
}

void ShellActivity::onPause() {
    m_shell->getPageHost()->pauseCurrentPage();
}

void ShellActivity::onResume() {
    m_shell->getPageHost()->resumeCurrentPage();
}

bool ShellActivity::handleBack() {
    Audio::instance()->play(SoundEffect::Enter);

    auto* pageHost = m_shell->getPageHost();
    if (pageHost->canPop()) return pageHost->popPage();

    showExitConfirmation();
    return true;
}

void ShellActivity::showExitConfirmation() {
    CustomDialog::show(brls::getStr("view/shell/exitConfirm"), {
        {brls::getStr("view/dialog/cancel"), [] { CustomDialog::close(); }},
        {brls::getStr("view/dialog/confirm"), [] { CustomDialog::close([] { brls::Application::quit(); }); }},
    });
}
