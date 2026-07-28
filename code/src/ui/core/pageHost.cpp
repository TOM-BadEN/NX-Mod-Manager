/**
 * PageHost - 普通 Page 的内容容器
 */

#include "ui/core/pageHost.hpp"
#include <algorithm>

PageHost::PageHost() {
    setClipsToBounds(true);
}

PageHost::~PageHost() {
    // 动画返回期间，待移除页面已不在 m_pageStack 中，因此大退时不会在此收到 willDisappear。
    // 它们仍保留在 Box 中并会正确销毁；以后若在 willDisappear 中加入必要清理，需要同步处理此情况。
    for (auto it = m_pageStack.rbegin(); it != m_pageStack.rend(); ++it) {
        if (it->state == PageState::Disappeared) continue;

        it->page->willDisappear(true);
        it->state = PageState::Disappeared;
    }

    // Box 按子 View 的存放顺序销毁，退出时调整为页面栈顶到栈底。
    auto& pages = getChildren();
    std::reverse(pages.begin(), pages.end());
}

bool PageHost::setRootPage(Page* page) {
    if (!page || !m_pageStack.empty() || page->hasParent()) return false;

    // 对照 Activity：页面先获得内容区域尺寸，再执行 onContentAvailable。
    page->setDimensions(getWidth(), getHeight());
    page->onContentAvailable();
    preparePage(page);
    brls::Box::addView(page);
    m_pageStack.push_back({page, PageState::Visible});

    notifyCurrentPageChanged();
    return true;
}

bool PageHost::pushPage(Page* page, PageAnimType anim) {
    auto* current = getCurrentEntry();
    if (!page || !current || page->hasParent() || containsPage(page)) return false;
    if (current->state != PageState::Visible || m_pageAnim.isRunning()) return false;
    if (anim != PageAnimType::None && (getWidth() <= 0.0f || getHeight() <= 0.0f)) return false;

    Page* previousPage = current->page;
    current->page->onPause();
    if (anim == PageAnimType::None) current->page->hide([] {}, false, 0);
    current->state = PageState::Paused;

    // 对照 Activity：页面先获得内容区域尺寸，再执行 onContentAvailable。
    page->setDimensions(getWidth(), getHeight());
    page->onContentAvailable();
    preparePage(page);
    brls::Box::addView(page);
    m_pageStack.push_back({page, PageState::Visible});

    if (anim != PageAnimType::None) {
        brls::Application::blockInputs();
        if (!m_pageAnim.start(page, previousPage, anim, getWidth(), getHeight(), [previousPage] {
            // hide 只改变显示状态，不会额外触发 View 生命周期。
            previousPage->hide([] {}, false, 0);
            brls::Application::unblockInputs();
        })) {
            previousPage->hide([] {}, false, 0);
            brls::Application::unblockInputs();
        }
    }

    notifyCurrentPageChanged();
    brls::Application::giveFocus(page);

    return true;
}

bool PageHost::popPage(PageAnimType anim) {
    return popPages(1, anim);
}

bool PageHost::popPages(std::size_t count, PageAnimType anim) {
    if (count == 0 || count >= m_pageStack.size()) return false;
    if (m_pageAnim.isRunning()) return false;

    auto* current = getCurrentEntry();
    if (!current || current->state != PageState::Visible) return false;
    if (anim != PageAnimType::None && (getWidth() <= 0.0f || getHeight() <= 0.0f)) return false;

    std::size_t targetIndex = m_pageStack.size() - count - 1;
    for (std::size_t i = targetIndex; i < m_pageStack.size() - 1; i++) {
        if (m_pageStack[i].state != PageState::Paused) return false;
    }

    if (anim == PageAnimType::None) {
        // 中间页面保持暂停和隐藏，只恢复最终目标页面。
        for (std::size_t i = 0; i < count; i++) {
            Page* page = m_pageStack.back().page;
            brls::Box::removeView(page);
            m_pageStack.pop_back();
        }

        auto* previous = getCurrentEntry();
        previous->page->show([] {}, false, 0);
        previous->page->onResume();
        previous->state = PageState::Visible;

        notifyCurrentPageChanged();
        brls::Application::giveFocus(previous->page);

        return true;
    }

    Page* leavingPage = m_pageStack.back().page;
    Page* enteringPage = m_pageStack[targetIndex].page;

    // 动画期间保留待移除页面，结束后按栈顶到栈底的顺序销毁。
    std::vector<Page*> leavingPages;
    leavingPages.reserve(count);
    for (std::size_t i = 0; i < count; i++) {
        leavingPages.push_back(m_pageStack[m_pageStack.size() - i - 1].page);
    }
    m_pageStack.resize(targetIndex + 1);

    enteringPage->show([] {}, false, 0);
    enteringPage->onResume();
    m_pageStack.back().state = PageState::Visible;

    auto removeLeavingPages = [this, leavingPages] {
        for (auto* page : leavingPages) {
            brls::Box::removeView(page);
        }
        brls::Application::unblockInputs();
    };

    brls::Application::blockInputs();
    if (!m_pageAnim.start(enteringPage, leavingPage, anim, getWidth(), getHeight(), removeLeavingPages))
        removeLeavingPages();

    notifyCurrentPageChanged();
    brls::Application::giveFocus(enteringPage);

    return true;
}

bool PageHost::canPop() const {
    return m_pageStack.size() > 1;
}

Page* PageHost::getCurrentPage() const {
    auto* entry = getCurrentEntry();
    return entry ? entry->page : nullptr;
}

Page* PageHost::getPreviousPage() const {
    return m_pageStack.size() > 1 ? m_pageStack[m_pageStack.size() - 2].page : nullptr;
}

std::size_t PageHost::getPageCount() const {
    return m_pageStack.size();
}

void PageHost::pauseCurrentPage() {
    auto* current = getCurrentEntry();
    if (!current || current->state != PageState::Visible) return;

    current->page->onPause();
    current->state = PageState::Paused;
}

void PageHost::resumeCurrentPage() {
    auto* current = getCurrentEntry();
    if (!current || current->state != PageState::Paused || current->page->isHidden()) return;

    current->page->onResume();
    current->state = PageState::Visible;
}

brls::Event<Page*>* PageHost::getCurrentPageChangedEvent() {
    return &m_currentPageChangedEvent;
}

brls::View* PageHost::getDefaultFocus() {
    auto* page = getCurrentPage();
    return page ? page->getDefaultFocus() : nullptr;
}

brls::View* PageHost::getNextFocus(brls::FocusDirection direction, brls::View*) {
    auto* next = getParentNavigationDecision(this, nullptr, direction);
    if (!next && hasParent()) next = getParent()->getNextFocus(direction, this);
    return next;
}

void PageHost::willAppear(bool resetState) {
    if (m_hostVisible) return;
    m_hostVisible = true;

    auto* current = getCurrentEntry();
    if (!current || current->state != PageState::Disappeared) return;

    current->page->willAppear(resetState);
    current->state = PageState::Visible;
}

void PageHost::willDisappear(bool resetState) {
    if (!m_hostVisible) return;
    m_hostVisible = false;

    auto* current = getCurrentEntry();
    if (!current || current->state == PageState::Disappeared) return;

    current->page->willDisappear(resetState);
    current->state = PageState::Disappeared;
}

brls::View* PageHost::create() {
    return new PageHost();
}

bool PageHost::containsPage(Page* page) const {
    return std::find_if(m_pageStack.begin(), m_pageStack.end(), [page](const PageEntry& entry) {
        return entry.page == page;
    }) != m_pageStack.end();
}

bool PageHost::isPageActive(const Page* page) const {
    auto* current = getCurrentEntry();
    return m_hostVisible && current && current->page == page && current->state == PageState::Visible;
}

void PageHost::preparePage(Page* page) {
    page->setPositionType(brls::PositionType::ABSOLUTE);
    page->setPositionTop(0);
    page->setPositionLeft(0);
    page->setWidthPercentage(100);
    page->setHeightPercentage(100);
}

PageHost::PageEntry* PageHost::getCurrentEntry() {
    return m_pageStack.empty() ? nullptr : &m_pageStack.back();
}

const PageHost::PageEntry* PageHost::getCurrentEntry() const {
    return m_pageStack.empty() ? nullptr : &m_pageStack.back();
}

void PageHost::notifyCurrentPageChanged() {
    m_currentPageChangedEvent.fire(getCurrentPage());
}
