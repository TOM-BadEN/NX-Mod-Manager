/**
 * Page - 全局 UI Shell 中的普通内容页面基类
 */

#include "ui/core/page.hpp"
#include "ui/core/pageHost.hpp"
#include "ui/view/shell/appShell.hpp"

namespace {

AppShell* getAppShell(Page* page) {
    auto* pageHost = dynamic_cast<PageHost*>(page->getParent());
    return pageHost ? dynamic_cast<AppShell*>(pageHost->getParent()) : nullptr;
}

} // namespace

Page::Page() = default;

Page::~Page() = default;

void Page::onContentAvailable() {
}

bool Page::isActive() {
    auto* pageHost = dynamic_cast<PageHost*>(getParent());
    return pageHost && pageHost->isPageActive(this);
}

void Page::willAppear(bool resetState) {
    brls::Box::willAppear(resetState);
}

void Page::onPause() {
}

void Page::onResume() {
}

void Page::willDisappear(bool resetState) {
    brls::Box::willDisappear(resetState);
}

void Page::pushPage(Page* page, PageAnimType anim) {
    auto* pageHost = dynamic_cast<PageHost*>(getParent());
    if (pageHost && pageHost->pushPage(page, anim)) return;

    if (page && !page->hasParent()) delete page;
    brls::fatal("Page::pushPage failed");
}

void Page::popPage(PageAnimType anim) {
    auto* pageHost = dynamic_cast<PageHost*>(getParent());
    if (pageHost && pageHost->popPage(anim)) return;

    brls::fatal("Page::popPage failed");
}

void Page::popPages(std::size_t count, PageAnimType anim) {
    auto* pageHost = dynamic_cast<PageHost*>(getParent());
    if (pageHost && pageHost->popPages(count, anim)) return;

    brls::fatal("Page::popPages failed");
}

void Page::setShowFps(bool show) {
    auto* appShell = getAppShell(this);
    if (appShell) appShell->setShowFps(show);
}

void Page::setShowMem(bool show) {
    auto* appShell = getAppShell(this);
    if (appShell) appShell->setShowMem(show);
}

void Page::shakeHeaderNav(bool right) {
    auto* appShell = getAppShell(this);
    if (appShell) appShell->shakeHeaderNav(right);
}
