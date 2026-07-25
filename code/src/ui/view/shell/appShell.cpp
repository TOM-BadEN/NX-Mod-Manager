/**
 * AppShell - 全局常驻 UI 外壳
 */

#include "ui/view/shell/appShell.hpp"

AppShell::AppShell() {
    inflateFromXMLRes("xml/view/shell/appShell.xml");

    m_header = new GlobalHeader();
    m_pageHost = new PageHost();
    m_footer = new GlobalFooter();

    m_pageHost->setWidth(brls::View::AUTO);
    m_pageHost->setHeight(brls::View::AUTO);
    m_pageHost->setGrow(1.0f);

    brls::Box::addView(m_header);
    brls::Box::addView(m_pageHost);
    brls::Box::addView(m_footer);

    m_pageChangedSubscription = m_pageHost->getCurrentPageChangedEvent()->subscribe([this](Page* page) {
        bindShellState(page);
    });

    applyShellState({});
}

AppShell::~AppShell() {
    if (m_shellState) {
        m_shellState->getStateChangedEvent()->unsubscribe(m_stateChangedSubscription);
        m_shellState = nullptr;
    }

    m_pageHost->getCurrentPageChangedEvent()->unsubscribe(m_pageChangedSubscription);
}

PageHost* AppShell::getPageHost() const {
    return m_pageHost;
}

void AppShell::setShowFps(bool show) {
    m_header->setShowFps(show);
}

void AppShell::setShowMem(bool show) {
    m_header->setShowMem(show);
}

brls::View* AppShell::create() {
    return new AppShell();
}

void AppShell::bindShellState(Page* page) {
    if (m_shellState) {
        m_shellState->getStateChangedEvent()->unsubscribe(m_stateChangedSubscription);
        m_shellState = nullptr;
    }

    m_shellState = dynamic_cast<ShellState*>(page);
    if (!m_shellState) {
        applyShellState({});
        return;
    }

    m_stateChangedSubscription = m_shellState->getStateChangedEvent()->subscribe([this](const ShellStateData& state) {
        applyShellState(state);
    });

    applyShellState(m_shellState->getState());
}

void AppShell::applyShellState(const ShellStateData& state) {
    m_header->setTitle(state.title);
    m_header->setSubtitle(state.subtitle);
    m_footer->setIndexText(state.indexText);
}
