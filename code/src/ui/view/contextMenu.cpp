/**
 * ContextMenu - 右侧上下文菜单
 */

#include "ui/view/contextMenu.hpp"
#include "core/audio.hpp"
#include "ui/view/contextMenuCell.hpp"
#include "ui/view/dialog/customDialog.hpp"
#include <algorithm>
#include <borealis/core/i18n.hpp>
#include <unordered_set>
#include <utility>

namespace {
constexpr int DRAWER_OPEN_MS = 200;
constexpr int DRAWER_CLOSE_MS = 130;

void invokeCallback(const std::function<void()>& callback) {
    if (callback) callback();
}
}

struct ContextMenuPageData {
    explicit ContextMenuPageData(const std::string& title) : title(title) {}

    std::string title;
    std::string icon;
    std::vector<std::unique_ptr<ContextMenuEntry>> entries;
    std::function<void()> onDismiss;
    std::function<bool()> showFakeHighlight;
};

struct ContextMultiSelectPageData {
    explicit ContextMultiSelectPageData(const std::string& title) : title(title) {}

    std::string title;
    std::string icon;
    std::vector<std::unique_ptr<ContextMultiSelectOption>> options;
    std::function<void(const std::vector<int>&)> confirm;
};

// Borealis 的 push/pop 只通过 Activity 暴露暂停与恢复通知。
class ContextMenu::ActivityHost final : public brls::Activity {
public:
    explicit ActivityHost(ContextMenu* menu) : brls::Activity(menu), m_menu(menu) {}

    void onPause() override {
        m_menu->onActivityPause();
    }

    void onResume() override {
        m_menu->onActivityResume();
    }

private:
    ContextMenu* m_menu;
};

ContextMenuEntry::ContextMenuEntry(const std::string& title, const std::string& hint)
    : m_title(title), m_hint(hint) {}

void ContextMenuEntry::setBadge(const std::string& badge) {
    m_badge = badge;
    m_badgeProvider = nullptr;
}

void ContextMenuEntry::setBadge(std::function<std::string()> badge) {
    m_badgeProvider = std::move(badge);
}

void ContextMenuEntry::setIcon(const std::string& path) {
    m_icon = path;
}

void ContextMenuEntry::setDisabled(std::function<bool()> disabled) {
    m_disabledProvider = std::move(disabled);
}

void ContextMenuEntry::setBadgeHighlight(std::function<bool()> highlighted) {
    m_badgeHighlightProvider = std::move(highlighted);
}

const std::string& ContextMenuEntry::title() const {
    return m_title;
}

const std::string& ContextMenuEntry::hint() const {
    return m_hint;
}

const std::string& ContextMenuEntry::icon() const {
    return m_icon;
}

std::string ContextMenuEntry::badge() const {
    return m_badgeProvider ? m_badgeProvider() : m_badge;
}

bool ContextMenuEntry::disabled() const {
    return m_disabledProvider ? m_disabledProvider() : false;
}

bool ContextMenuEntry::badgeHighlighted() const {
    return m_badgeHighlightProvider ? m_badgeHighlightProvider() : true;
}

ContextMenuBehaviorEntry::ContextMenuBehaviorEntry(const std::string& title, const std::string& hint)
    : ContextMenuEntry(title, hint) {}

void ContextMenuBehaviorEntry::setBack() {
    m_afterAction = AfterAction::Back;
}

void ContextMenuBehaviorEntry::setStayOpen() {
    m_afterAction = AfterAction::Stay;
}

ContextMenuBehaviorEntry::AfterAction ContextMenuBehaviorEntry::afterAction() const {
    return m_afterAction;
}

ContextMenuActionEntry::ContextMenuActionEntry(const std::string& title, const std::string& hint)
    : ContextMenuBehaviorEntry(title, hint) {}

void ContextMenuActionEntry::onSelected(std::function<void()> selected) {
    m_selected = std::move(selected);
}

ContextMenuEntry::Kind ContextMenuActionEntry::kind() const {
    return Kind::Action;
}

std::function<void()> ContextMenuActionEntry::selectedListener() const {
    return m_selected;
}

ContextMenuTaskEntryBase::ContextMenuTaskEntryBase(const std::string& title, const std::string& hint)
    : ContextMenuBehaviorEntry(title, hint) {}

void ContextMenuTaskEntryBase::setCancelable(bool cancelable) {
    m_cancelable = cancelable;
}

ContextMenuEntry::Kind ContextMenuTaskEntryBase::kind() const {
    return Kind::Task;
}

bool ContextMenuTaskEntryBase::cancelable() const {
    return m_cancelable;
}

ContextMenuSwitchEntry::ContextMenuSwitchEntry(const std::string& title, const std::string& hint)
    : ContextMenuEntry(title, hint) {}

void ContextMenuSwitchEntry::setState(std::function<bool()> state) {
    m_state = std::move(state);
}

void ContextMenuSwitchEntry::setTask(std::function<void(bool)> task) {
    m_task = std::move(task);
}

void ContextMenuSwitchEntry::setEnableConfirmation(const std::string& message) {
    m_enableConfirmation = message;
}

void ContextMenuSwitchEntry::setDisableConfirmation(const std::string& message) {
    m_disableConfirmation = message;
}

ContextMenuEntry::Kind ContextMenuSwitchEntry::kind() const {
    return Kind::Switch;
}

std::function<bool()> ContextMenuSwitchEntry::stateReader() const {
    return m_state;
}

std::function<void(bool)> ContextMenuSwitchEntry::task() const {
    return m_task;
}

const std::string& ContextMenuSwitchEntry::confirmation(bool requestedState) const {
    return requestedState ? m_enableConfirmation : m_disableConfirmation;
}

ContextMenuRadioEntry::ContextMenuRadioEntry(const std::string& title, const std::string& hint)
    : ContextMenuBehaviorEntry(title, hint) {}

void ContextMenuRadioEntry::setSelected(std::function<bool()> selected) {
    m_selectedReader = std::move(selected);
}

void ContextMenuRadioEntry::onSelected(std::function<void()> selected) {
    m_selected = std::move(selected);
}

ContextMenuEntry::Kind ContextMenuRadioEntry::kind() const {
    return Kind::Radio;
}

std::function<bool()> ContextMenuRadioEntry::selectedReader() const {
    return m_selectedReader;
}

std::function<void()> ContextMenuRadioEntry::selectedListener() const {
    return m_selected;
}

ContextMenuSubmenuEntry::ContextMenuSubmenuEntry(const std::string& title, const std::string& hint)
    : ContextMenuEntry(title, hint) {}

void ContextMenuSubmenuEntry::setPage(const ContextMenuPage& page) {
    if (wouldCreateCycle(page.m_data)) {
        brls::Logger::error("ContextMenu: submenu page cycle rejected");
        return;
    }
    m_page = page.m_data;
}

ContextMenuEntry::Kind ContextMenuSubmenuEntry::kind() const {
    return Kind::Submenu;
}

std::shared_ptr<ContextMenuPageData> ContextMenuSubmenuEntry::page() const {
    return m_page;
}

bool ContextMenuSubmenuEntry::wouldCreateCycle(const std::shared_ptr<ContextMenuPageData>& page) const {
    auto ownerPage = m_ownerPage.lock();
    if (!ownerPage || !page) return false;

    std::vector<std::shared_ptr<ContextMenuPageData>> pending{page};
    std::unordered_set<const ContextMenuPageData*> visited;
    while (!pending.empty()) {
        auto current = std::move(pending.back());
        pending.pop_back();
        if (current.get() == ownerPage.get()) return true;
        if (!visited.insert(current.get()).second) continue;

        for (const auto& entry : current->entries) {
            auto* submenu = dynamic_cast<ContextMenuSubmenuEntry*>(entry.get());
            if (submenu && submenu->m_page) pending.push_back(submenu->m_page);
        }
    }
    return false;
}

ContextMultiSelectOption::ContextMultiSelectOption(const std::string& title, const std::string& hint)
    : ContextMenuEntry(title, hint) {}

void ContextMultiSelectOption::setSelected(bool selected) {
    m_selected = selected;
}

ContextMenuEntry::Kind ContextMultiSelectOption::kind() const {
    return Kind::MultiSelectOption;
}

bool ContextMultiSelectOption::selected() const {
    return m_selected;
}

ContextMenuPage::ContextMenuPage(const std::string& title) : m_data(std::make_shared<ContextMenuPageData>(title)) {}

void ContextMenuPage::appendEntry(std::unique_ptr<ContextMenuEntry> entry) {
    m_data->entries.push_back(std::move(entry));
}

ContextMenuActionEntry& ContextMenuPage::addAction(const std::string& title, const std::string& hint) {
    auto* entry = new ContextMenuActionEntry(title, hint);
    appendEntry(std::unique_ptr<ContextMenuEntry>(entry));
    return *entry;
}

ContextMenuSwitchEntry& ContextMenuPage::addSwitch(const std::string& title, const std::string& hint) {
    auto* entry = new ContextMenuSwitchEntry(title, hint);
    appendEntry(std::unique_ptr<ContextMenuEntry>(entry));
    return *entry;
}

ContextMenuRadioEntry& ContextMenuPage::addRadio(const std::string& title, const std::string& hint) {
    auto* entry = new ContextMenuRadioEntry(title, hint);
    appendEntry(std::unique_ptr<ContextMenuEntry>(entry));
    return *entry;
}

ContextMenuSubmenuEntry& ContextMenuPage::addSubmenu(const std::string& title, const std::string& hint) {
    auto* entry = new ContextMenuSubmenuEntry(title, hint);
    entry->m_ownerPage = m_data;
    appendEntry(std::unique_ptr<ContextMenuEntry>(entry));
    return *entry;
}

void ContextMenuPage::setIcon(const std::string& path) {
    m_data->icon = path;
}

void ContextMenuPage::setOnDismiss(std::function<void()> onDismiss) {
    m_data->onDismiss = std::move(onDismiss);
}

void ContextMenuPage::setShowFakeHighlight(std::function<bool()> show) {
    m_data->showFakeHighlight = std::move(show);
}

void ContextMenuPage::show() const {
    brls::View* previousFocus = nullptr;
    if (!m_data->showFakeHighlight || m_data->showFakeHighlight())
        previousFocus = brls::Application::getCurrentFocus();

    auto* menu = new ContextMenu(m_data, previousFocus);
    brls::Application::pushActivity(new ContextMenu::ActivityHost(menu), brls::TransitionAnimation::NONE);
}

ContextMultiSelectPage::ContextMultiSelectPage(const std::string& title) : m_data(std::make_shared<ContextMultiSelectPageData>(title)) {}

ContextMultiSelectOption& ContextMultiSelectPage::addOption(const std::string& title, const std::string& hint) {
    auto* option = new ContextMultiSelectOption(title, hint);
    auto ownedOption = std::unique_ptr<ContextMultiSelectOption>(option);
    m_data->options.push_back(std::move(ownedOption));
    return *option;
}

void ContextMultiSelectPage::onConfirm(std::function<void(const std::vector<int>&)> confirm) {
    m_data->confirm = std::move(confirm);
}

void ContextMultiSelectPage::setIcon(const std::string& path) {
    m_data->icon = path;
}

void ContextMultiSelectPage::show() const {
    auto* menu = new ContextMenu(m_data, brls::Application::getCurrentFocus());
    brls::Application::pushActivity(new ContextMenu::ActivityHost(menu), brls::TransitionAnimation::NONE);
}

class ContextMenuDataSource final : public RecyclingGridDataSource {
public:
    explicit ContextMenuDataSource(ContextMenu* menu) : m_menu(menu) {}

    size_t getItemCount() override {
        return m_menu->itemCount();
    }

    RecyclingGridItem* cellForRow(RecyclingGrid* grid, size_t index) override {
        auto* cell = static_cast<ContextMenuCell*>(grid->dequeueReusableCell("ContextMenuCell"));
        m_menu->bindCell(*cell, index);
        return cell;
    }

    void onItemSelected(RecyclingGrid*, size_t index) override {
        m_menu->handleSelection(index);
    }

    void clearData() override {}

private:
    ContextMenu* m_menu;
};

ContextMenu::ContextMenu(std::shared_ptr<ContextMenuPageData> rootPage, brls::View* previousFocus)
    : m_rootPage(std::move(rootPage)) {
    if (previousFocus) {
        m_previousFocusFrame = FocusFrame{
            previousFocus->getX(), previousFocus->getY(),
            previousFocus->getWidth(), previousFocus->getHeight()};
    }
    initialize();
}

ContextMenu::ContextMenu(std::shared_ptr<ContextMultiSelectPageData> multiPage, brls::View* previousFocus)
    : m_multiPage(std::move(multiPage)) {
    if (previousFocus) {
        m_previousFocusFrame = FocusFrame{
            previousFocus->getX(), previousFocus->getY(),
            previousFocus->getWidth(), previousFocus->getHeight()};
    }
    initialize();
}

ContextMenu::~ContextMenu() {
    m_session.reset();
    m_viewSession.reset();
    m_closing = true;
    m_stopSource.request_stop();
    m_runningTask.wait();

    releaseDrawerInputBlock();
    if (m_drawerAnimation.isRunning()) m_drawerAnimation.stop();
    m_drawerDone = nullptr;
}

void ContextMenu::initialize() {
    inflateFromXMLRes("xml/view/contextMenu.xml");

    m_drawerAnimation.setTickCallback([this] { updateDrawerViews(); });
    m_drawerAnimation.setEndCallback([this](bool finished) { finishDrawer(finished); });

    m_version->setText(APP_VERSION);
    m_hintCard->setVisibility(brls::Visibility::INVISIBLE);

    m_grid->setPadding(6, 35, 6, 28);
    m_grid->registerCell("ContextMenuCell", ContextMenuCell::create);
    m_grid->setFocusChangeCallback([this](size_t index) {
        updateHint(index);
        updateIndex(index);
    });
    m_grid->setDataSource(new ContextMenuDataSource(this));

    registerAction("", brls::ControllerButton::BUTTON_B, [this](brls::View*) {
        Audio::instance()->play(SoundEffect::Enter);
        if (m_closing) return true;

        if (m_runningKind != RunningKind::None) {
            if (m_runningKind == RunningKind::Task && m_activeTaskCancelable)
                m_stopSource.request_stop();
            return true;
        }

        popPage(true);
        return true;
    }, true);

    if (isMultiSelect()) {
        registerAction(
            brls::getStr("view/contextMenu/submit"),
            brls::ControllerButton::BUTTON_START,
            [this](brls::View*) {
                submitMultiSelect();
                return true;
            });
        setActionAvailable(brls::ControllerButton::BUTTON_START, false);
    }

    m_left->addGestureRecognizer(new brls::TapGestureRecognizer(
        [this](brls::TapGestureStatus status, brls::Sound*) {
            if (status.state != brls::GestureState::END) return;
            if (m_closing || m_runningKind != RunningKind::None) return;
            if (m_hintCard->getVisibility() == brls::Visibility::VISIBLE &&
                m_hintCard->getFrame().pointInside(status.position))
                return;

            Audio::instance()->play(SoundEffect::Enter);
            closeMenu(true);
        }));
}

void ContextMenu::willAppear(bool resetState) {
    Box::willAppear(resetState);
    if (m_initialized) return;
    m_initialized = true;

    if (isMultiSelect()) {
        m_multiSelected.reserve(m_multiPage->options.size());
        for (const auto& option : m_multiPage->options) {
            m_multiSelected.push_back(option->selected());
            if (option->selected()) m_multiSelectedCount++;
        }

        configureCurrentPage();
        m_grid->setDefaultCellFocus(0);
        m_grid->reloadData();
        if (itemCount() > 0) {
            m_grid->instantFocus(0);
            setScrollOffsetExact(0.0f);
            updateHint(0);
            updateIndex(0);
        } else {
            setScrollOffsetExact(0.0f);
            m_hintCard->setVisibility(brls::Visibility::INVISIBLE);
            m_index->setText("0 / 0");
        }
        setActionAvailable(brls::ControllerButton::BUTTON_START, m_multiSelectedCount > 0);
    } else {
        pushPage(m_rootPage);
    }

    startDrawerOpen();
}

void ContextMenu::willDisappear(bool resetState) {
    invalidateSession();
    m_viewSession.reset();
    m_stopSource.request_stop();
    releaseDrawerInputBlock();
    Box::willDisappear(resetState);
}

void ContextMenu::invalidateSession() {
    m_closing = true;
    m_session.reset();
}

void ContextMenu::onActivityPause() {
    m_activityPaused = true;
}

void ContextMenu::onActivityResume() {
    m_activityPaused = false;
    if (m_popActivityPending || m_backPending)
        schedulePendingActivityCheck();
}

bool ContextMenu::isTopActivity() {
    auto activities = brls::Application::getActivitiesStack();
    return !activities.empty() && activities.back() == getParentActivity();
}

void ContextMenu::schedulePendingActivityCheck() {
    if (m_activityCheckScheduled) return;
    m_activityCheckScheduled = true;

    // Borealis 先 onResume、后移除上层 Activity，必须等栈顶身份真正匹配。
    std::weak_ptr<Session> viewSession = m_viewSession;
    auto* menu = this;
    brls::sync([viewSession, menu] {
        auto activeView = viewSession.lock();
        if (!activeView) return;
        // 不让临时强引用跨过可能销毁菜单的页面行为。
        activeView.reset();

        menu->m_activityCheckScheduled = false;
        if (menu->m_activityPaused) return;
        if (!menu->isTopActivity()) {
            menu->schedulePendingActivityCheck();
            return;
        }
        menu->processPendingActivityWork();
    });
}

void ContextMenu::processPendingActivityWork() {
    if (m_popActivityPending) {
        auto afterClose = std::move(m_pendingAfterClose);
        m_popActivityPending = false;
        m_backPending = false;
        popMenuActivity(std::move(afterClose));
        return;
    }

    if (m_backPending) {
        m_backPending = false;
        requestBack(true);
    }
}

// 菜单打开后底层控件不再拥有焦点，在背景上保留其焦点边框。
void ContextMenu::draw(NVGcontext* vg, float x, float y, float width, float height,
    brls::Style style, brls::FrameContext* ctx) {
    if (m_previousFocusFrame &&
        brls::Application::getInputType() != brls::InputType::TOUCH) {
        float strokeWidth = style["brls/highlight/stroke_width"];
        float cornerRadius = style["brls/highlight/corner_radius"];
        auto theme = brls::Application::getTheme();
        float focusX = m_previousFocusFrame->x - strokeWidth / 2;
        float focusY = m_previousFocusFrame->y - strokeWidth / 2;
        float focusWidth = m_previousFocusFrame->width + strokeWidth;
        float focusHeight = m_previousFocusFrame->height + strokeWidth;

        float shadowOffset = style["brls/highlight/shadow_offset"];
        auto shadowPaint = nvgBoxGradient(
            vg, focusX, focusY + style["brls/highlight/shadow_width"],
            focusWidth, focusHeight, cornerRadius * 2,
            style["brls/highlight/shadow_feather"],
            nvgRGBA(0, 0, 0, static_cast<int>(style["brls/highlight/shadow_opacity"])),
            brls::TRANSPARENT);
        nvgBeginPath(vg);
        nvgRect(vg, focusX - shadowOffset, focusY - shadowOffset,
            focusWidth + shadowOffset * 2, focusHeight + shadowOffset * 3);
        nvgRoundedRect(vg, focusX, focusY, focusWidth, focusHeight, cornerRadius);
        nvgPathWinding(vg, NVG_HOLE);
        nvgFillPaint(vg, shadowPaint);
        nvgFill(vg);

        float gradientX;
        float gradientY;
        float colorAnimation;
        brls::getHighlightAnimation(&gradientX, &gradientY, &colorAnimation);

        auto pulseColor = theme["brls/highlight/color1"];
        pulseColor.a *= 0.85f + colorAnimation * 0.15f;
        nvgBeginPath(vg);
        nvgStrokeColor(vg, pulseColor);
        nvgStrokeWidth(vg, strokeWidth);
        nvgRoundedRect(vg, focusX, focusY, focusWidth, focusHeight, cornerRadius);
        nvgStroke(vg);

        auto borderColor = theme["brls/highlight/color2"];
        borderColor.a = 0.5f;
        auto borderFirst = nvgRadialGradient(
            vg, focusX + gradientX * focusWidth, focusY + gradientY * focusHeight,
            strokeWidth * 10, strokeWidth * 40, borderColor, brls::TRANSPARENT);
        nvgBeginPath(vg);
        nvgStrokePaint(vg, borderFirst);
        nvgStrokeWidth(vg, strokeWidth);
        nvgRoundedRect(vg, focusX, focusY, focusWidth, focusHeight, cornerRadius);
        nvgStroke(vg);

        auto borderSecond = nvgRadialGradient(
            vg, focusX + (1 - gradientX) * focusWidth,
            focusY + (1 - gradientY) * focusHeight,
            strokeWidth * 10, strokeWidth * 40, borderColor, brls::TRANSPARENT);
        nvgBeginPath(vg);
        nvgStrokePaint(vg, borderSecond);
        nvgStrokeWidth(vg, strokeWidth);
        nvgRoundedRect(vg, focusX, focusY, focusWidth, focusHeight, cornerRadius);
        nvgStroke(vg);
    }

    Box::draw(vg, x, y, width, height, style, ctx);
}

bool ContextMenu::isMultiSelect() const {
    return static_cast<bool>(m_multiPage);
}

size_t ContextMenu::itemCount() const {
    if (isMultiSelect()) return m_multiPage->options.size();
    if (m_pageStack.empty()) return 0;
    return m_pageStack.back().page->entries.size();
}

ContextMenuEntry* ContextMenu::itemAt(size_t index) const {
    if (index >= itemCount()) return nullptr;
    if (isMultiSelect()) return m_multiPage->options[index].get();
    return m_pageStack.back().page->entries[index].get();
}

void ContextMenu::pushPage(std::shared_ptr<ContextMenuPageData> page) {
    if (!page || m_closing) return;

    if (!m_pageStack.empty()) {
        auto& current = m_pageStack.back();
        current.focusIndex = m_grid->getDefaultCellFocus();
        current.scrollOffset = m_grid->getContentOffsetY();
    }

    m_pageStack.push_back({std::move(page), 0, 0.0f});
    configureCurrentPage();

    m_grid->setDefaultCellFocus(0);
    m_grid->reloadData();
    if (itemCount() > 0) {
        m_grid->instantFocus(0);
        setScrollOffsetExact(0.0f);
        updateHint(0);
        updateIndex(0);
    } else {
        setScrollOffsetExact(0.0f);
        m_hintCard->setVisibility(brls::Visibility::INVISIBLE);
        m_index->setText("0 / 0");
    }
}

void ContextMenu::popPage(bool animated) {
    if (m_closing || m_runningKind != RunningKind::None) return;
    if (isMultiSelect() || m_pageStack.size() <= 1) {
        closeMenu(animated);
        return;
    }

    m_pageStack.pop_back();
    auto& parent = m_pageStack.back();
    configureCurrentPage();

    size_t count = itemCount();
    size_t focusIndex = count == 0 ? 0 : std::min(parent.focusIndex, count - 1);
    float scrollOffset = parent.scrollOffset;

    m_grid->setDefaultCellFocus(focusIndex);
    m_grid->reloadData();
    if (count > 0) {
        m_grid->instantFocus(focusIndex);
        setScrollOffsetExact(scrollOffset);
        updateHint(focusIndex);
        updateIndex(focusIndex);
    } else {
        setScrollOffsetExact(0.0f);
        m_hintCard->setVisibility(brls::Visibility::INVISIBLE);
        m_index->setText("0 / 0");
    }
}

void ContextMenu::requestBack(bool unlockInteraction) {
    if (m_closing) return;
    if (!isTopActivity()) {
        // 页面恢复前保持锁定，避免 onResume 到延迟返回之间的一帧误操作。
        m_grid->setInteractionLocked(true);
        m_backPending = true;
        if (!m_activityPaused) schedulePendingActivityCheck();
        return;
    }

    std::weak_ptr<Session> session = m_session;
    popPage(false);
    if (unlockInteraction && !session.expired()) m_grid->setInteractionLocked(false);
}

void ContextMenu::configureCurrentPage() {
    const std::string* title;
    const std::string* icon;

    if (isMultiSelect()) {
        title = &m_multiPage->title;
        icon = &m_multiPage->icon;
        m_panel->setWidthPercentage(40.0f);
    } else {
        title = &m_pageStack.back().page->title;
        icon = &m_pageStack.back().page->icon;
        m_panel->setWidthPercentage(37.0f);
    }

    m_title->setText(*title);
    m_title->setAnimated(true);
    updateTitleIcon(*icon);
}

void ContextMenu::setScrollOffsetExact(float offset) {
    // 相同数值会被 ScrollingFrame 提前忽略，先偏移一像素以停止旧动画。
    if (m_grid->getContentOffsetY() == offset) m_grid->setContentOffsetY(offset + 1.0f, false);
    m_grid->setContentOffsetY(offset, false);
}

void ContextMenu::handleSelection(size_t index) {
    if (m_closing || m_runningKind != RunningKind::None) return;

    auto* entry = itemAt(index);
    if (!entry || entry->disabled()) return;

    if (isMultiSelect()) {
        toggleMultiSelect(index);
        return;
    }

    switch (entry->kind()) {
        case ContextMenuEntry::Kind::Action:
            handleAction(static_cast<ContextMenuActionEntry&>(*entry));
            break;
        case ContextMenuEntry::Kind::Task:
            startTask(index, static_cast<ContextMenuTaskEntryBase&>(*entry));
            break;
        case ContextMenuEntry::Kind::Switch: {
            auto* cell = dynamic_cast<ContextMenuCell*>(m_grid->getGridItemByIndex(index));
            if (cell && cell->isSwitchAnimating()) return;
            handleSwitch(index, static_cast<ContextMenuSwitchEntry&>(*entry));
            break;
        }
        case ContextMenuEntry::Kind::Radio:
            handleRadio(index, static_cast<ContextMenuRadioEntry&>(*entry));
            break;
        case ContextMenuEntry::Kind::Submenu:
            pushPage(static_cast<ContextMenuSubmenuEntry&>(*entry).page());
            break;
        case ContextMenuEntry::Kind::MultiSelectOption:
            break;
    }
}

void ContextMenu::handleAction(ContextMenuActionEntry& entry) {
    finishBehavior(entry.afterAction(), entry.selectedListener());
}

void ContextMenu::handleRadio(size_t index, ContextMenuRadioEntry& entry) {
    auto reader = entry.selectedReader();
    bool beforeState = reader ? reader() : false;
    finishBehavior(entry.afterAction(), entry.selectedListener(),
        RadioAnimation{index, beforeState});
}

void ContextMenu::finishBehavior(
    ContextMenuBehaviorEntry::AfterAction afterAction,
    std::function<void()> listener,
    std::optional<RadioAnimation> radioAnimation) {
    if (afterAction == ContextMenuBehaviorEntry::AfterAction::Close) {
        closeMenu(false, std::move(listener));
        return;
    }

    std::weak_ptr<Session> session = m_session;
    invokeCallback(listener);
    if (session.expired()) return;

    if (afterAction == ContextMenuBehaviorEntry::AfterAction::Back) {
        if (m_pageStack.size() <= 1)
            closeMenu(false);
        else
            requestBack(false);
        return;
    }

    refreshVisibleCells(std::nullopt, radioAnimation);
}

void ContextMenu::startTask(size_t index, ContextMenuTaskEntryBase& entry) {
    if (!entry.hasTask()) return;

    auto worker = entry.makeWorker();
    auto afterAction = entry.afterAction();
    m_stopSource = std::stop_source{};
    auto token = m_stopSource.get_token();

    m_runningKind = RunningKind::Task;
    m_activeIndex = index;
    m_activeTaskCancelable = entry.cancelable();
    size_t generation = ++m_runGeneration;

    m_grid->setInteractionLocked(true);
    refreshVisibleCell(index);

    std::weak_ptr<Session> session = m_session;
    auto* menu = this;
    m_runningTask = ThreadPool::instance().submitWaitable(
        [worker = std::move(worker), session, menu, generation, afterAction](
            std::stop_token workerToken) mutable {
            auto postResult = [session, menu, generation, afterAction, workerToken](
                                  bool completed, std::function<void()> complete) mutable {
                brls::sync([session, menu, generation, completed, workerToken,
                               afterAction, complete = std::move(complete)]() mutable {
                    auto activeSession = session.lock();
                    if (!activeSession) return;
                    // 用户监听可能重入关闭菜单，调用成员前不保留临时强引用。
                    activeSession.reset();
                    menu->finishTask(generation, completed, workerToken,
                        afterAction, std::move(complete));
                });
            };

            worker(workerToken, postResult);
        },
        token);
}

void ContextMenu::finishTask(
    size_t generation,
    bool completed,
    std::stop_token token,
    ContextMenuBehaviorEntry::AfterAction afterAction,
    std::function<void()> complete) {
    if (m_closing || generation != m_runGeneration ||
        m_runningKind != RunningKind::Task)
        return;

    bool normalCompletion = completed && !token.stop_requested();
    m_runningKind = RunningKind::None;
    m_activeIndex = NO_INDEX;
    m_activeTaskCancelable = false;

    if (!normalCompletion) {
        refreshVisibleCells();
        m_grid->setInteractionLocked(false);
        return;
    }

    if (afterAction == ContextMenuBehaviorEntry::AfterAction::Close) {
        refreshVisibleCells();
        closeMenu(false, std::move(complete));
        return;
    }

    std::weak_ptr<Session> session = m_session;
    invokeCallback(complete);
    if (session.expired()) return;
    refreshVisibleCells();

    if (afterAction == ContextMenuBehaviorEntry::AfterAction::Back) {
        if (m_pageStack.size() <= 1)
            closeMenu(false);
        else
            requestBack(true);
        return;
    }

    m_grid->setInteractionLocked(false);
}

void ContextMenu::handleSwitch(size_t index, ContextMenuSwitchEntry& entry) {
    auto stateReader = entry.stateReader();
    auto task = entry.task();
    if (!stateReader || !task) return;

    const auto& confirmation = entry.confirmation(!stateReader());
    if (confirmation.empty()) {
        startSwitch(index, entry);
        return;
    }

    auto* switchEntry = &entry;
    CustomDialog::show(confirmation, {
        {brls::getStr("view/dialog/cancel"), [] { CustomDialog::close(); }},
        {brls::getStr("view/dialog/confirm"), [this, index, switchEntry] {
            CustomDialog::close([this, index, switchEntry] {
                startSwitch(index, *switchEntry);
            });
        }},
    });
}

void ContextMenu::startSwitch(size_t index, ContextMenuSwitchEntry& entry) {
    auto stateReader = entry.stateReader();
    auto task = entry.task();
    if (!stateReader || !task) return;

    bool beforeState = stateReader();
    bool requestedState = !beforeState;

    m_stopSource = std::stop_source{};
    auto token = m_stopSource.get_token();
    m_runningKind = RunningKind::Switch;
    m_activeIndex = index;
    m_activeTaskCancelable = false;
    size_t generation = ++m_runGeneration;

    m_grid->setInteractionLocked(true);
    refreshVisibleCell(index);

    std::weak_ptr<Session> session = m_session;
    auto* menu = this;
    m_runningTask = ThreadPool::instance().submitWaitable(
        [task = std::move(task), requestedState, beforeState,
            stateReader = std::move(stateReader), session, menu, generation](
            std::stop_token) mutable {
            task(requestedState);
            brls::sync([session, menu, generation, beforeState, stateReader = std::move(stateReader)]() mutable {
                auto activeSession = session.lock();
                if (!activeSession) return;
                activeSession.reset();
                menu->finishSwitch(generation, beforeState, std::move(stateReader));
            });
        },
        token);
}

void ContextMenu::finishSwitch(size_t generation, bool beforeState, std::function<bool()> stateReader) {
    if (m_closing || generation != m_runGeneration ||
        m_runningKind != RunningKind::Switch)
        return;

    bool currentState = stateReader();

    size_t activeIndex = m_activeIndex;
    m_runningKind = RunningKind::None;
    m_activeIndex = NO_INDEX;

    refreshVisibleCells(SwitchAnimation{activeIndex, beforeState, currentState});
    m_grid->setInteractionLocked(false);
}

bool ContextMenu::isMultiSelected(size_t index) const {
    return index < m_multiSelected.size() && m_multiSelected[index];
}

void ContextMenu::toggleMultiSelect(size_t index) {
    if (index >= m_multiSelected.size()) return;

    bool beforeState = m_multiSelected[index];
    m_multiSelected[index] = !beforeState;
    if (m_multiSelected[index]) m_multiSelectedCount++;
    else m_multiSelectedCount--;

    auto* cell = dynamic_cast<ContextMenuCell*>(m_grid->getGridItemByIndex(index));
    if (cell) bindCell(*cell, index, std::nullopt, RadioAnimation{index, beforeState});

    setActionAvailable(brls::ControllerButton::BUTTON_START, m_multiSelectedCount > 0);
    updateIndex(m_grid->getDefaultCellFocus());
}

void ContextMenu::submitMultiSelect() {
    if (m_closing || m_runningKind != RunningKind::None ||
        m_multiSelectedCount == 0 || !m_multiPage->confirm)
        return;

    Audio::instance()->play(SoundEffect::Enter);

    std::vector<int> selected;
    selected.reserve(m_multiSelectedCount);
    for (size_t i = 0; i < m_multiSelected.size(); i++) {
        if (m_multiSelected[i]) selected.push_back(static_cast<int>(i));
    }

    auto confirm = m_multiPage->confirm;
    closeMenu(false, [confirm = std::move(confirm), selected = std::move(selected)] {
        confirm(selected);
    });
}

void ContextMenu::bindCell(
    ContextMenuCell& cell,
    size_t index,
    std::optional<SwitchAnimation> switchAnimation,
    std::optional<RadioAnimation> radioAnimation) {
    auto* entry = itemAt(index);
    if (!entry) return;

    bool disabled = entry->disabled();
    cell.bindBase(entry->title(), entry->icon(), disabled);
    cell.setLineBottom(index + 1 < itemCount() ? 1 : 0);

    if (m_runningKind == RunningKind::Task && index == m_activeIndex) {
        cell.showLoading();
        return;
    }

    switch (entry->kind()) {
        case ContextMenuEntry::Kind::Switch: {
            auto& switchEntry = static_cast<ContextMenuSwitchEntry&>(*entry);
            bool isAnimatedEntry = switchAnimation && switchAnimation->index == index;
            bool animated = isAnimatedEntry &&
                switchAnimation->beforeState != switchAnimation->afterState;
            bool previousState = animated ? switchAnimation->beforeState : false;
            bool state;
            if (isAnimatedEntry) {
                state = switchAnimation->afterState;
            } else {
                auto reader = switchEntry.stateReader();
                state = reader ? reader() : false;
            }
            cell.showSwitch(state, animated, previousState);
            break;
        }
        case ContextMenuEntry::Kind::Radio: {
            auto& radioEntry = static_cast<ContextMenuRadioEntry&>(*entry);
            auto reader = radioEntry.selectedReader();
            bool selected = reader ? reader() : false;
            bool animated = radioAnimation && radioAnimation->index == index &&
                radioAnimation->beforeState != selected;
            bool previousState = animated ? radioAnimation->beforeState : false;
            cell.showRadio(selected, animated, previousState);
            break;
        }
        case ContextMenuEntry::Kind::MultiSelectOption: {
            bool selected = isMultiSelected(index);
            bool animated = radioAnimation && radioAnimation->index == index &&
                radioAnimation->beforeState != selected;
            bool previousState = animated ? radioAnimation->beforeState : false;
            cell.showRadio(selected, animated, previousState);
            break;
        }
        case ContextMenuEntry::Kind::Action:
        case ContextMenuEntry::Kind::Task:
        case ContextMenuEntry::Kind::Submenu:
            cell.showBadge(entry->badge(), entry->badgeHighlighted());
            break;
    }
}

void ContextMenu::refreshVisibleCells(
    std::optional<SwitchAnimation> switchAnimation,
    std::optional<RadioAnimation> radioAnimation) {
    for (auto* item : m_grid->getGridItems()) {
        auto* cell = dynamic_cast<ContextMenuCell*>(item);
        if (cell) bindCell(*cell, cell->getIndex(), switchAnimation, radioAnimation);
    }
}

void ContextMenu::refreshVisibleCell(size_t index) {
    auto* cell = dynamic_cast<ContextMenuCell*>(m_grid->getGridItemByIndex(index));
    if (cell) bindCell(*cell, index);
}

void ContextMenu::updateHint(size_t index) {
    auto* entry = itemAt(index);
    if (!entry || entry->hint().empty()) {
        m_hintCard->setVisibility(brls::Visibility::INVISIBLE);
        return;
    }

    m_hintTitle->setText(entry->title());
    m_hint->setText(entry->hint());
    m_hintCard->setVisibility(brls::Visibility::VISIBLE);
}

void ContextMenu::updateIndex(size_t index) {
    size_t count = itemCount();
    if (count == 0) {
        m_index->setText("0 / 0");
        return;
    }

    index = std::min(index, count - 1);
    m_index->setText(std::to_string(index + 1) + " / " + std::to_string(count));
}

void ContextMenu::updateTitleIcon(const std::string& icon) {
    if (icon.empty()) {
        m_titleIcon->setVisibility(brls::Visibility::GONE);
        return;
    }

    m_titleIcon->setImageFromRes(icon);
    m_titleIcon->setVisibility(brls::Visibility::VISIBLE);
}

void ContextMenu::closeMenu(bool animated, std::function<void()> afterClose) {
    if (m_closing || m_drawerAnimation.isRunning()) return;

    invalidateSession();
    m_stopSource.request_stop();
    m_grid->setInteractionLocked(true);

    if (!animated) {
        popMenuActivity(std::move(afterClose));
        return;
    }

    startDrawerClose(std::move(afterClose));
}

void ContextMenu::popMenuActivity(std::function<void()> afterClose) {
    if (!isTopActivity()) {
        m_popActivityPending = true;
        m_pendingAfterClose = std::move(afterClose);
        if (!m_activityPaused) schedulePendingActivityCheck();
        return;
    }

    auto style = brls::Application::getStyle();
    float savedHighlight = style["brls/animations/highlight"];
    style.addMetric("brls/animations/highlight", 1.0f);
    std::function<void()> onDismiss;
    if (m_rootPage) onDismiss = m_rootPage->onDismiss;

    brls::Application::popActivity(
        brls::TransitionAnimation::NONE,
        [savedHighlight, afterClose = std::move(afterClose),
            onDismiss = std::move(onDismiss)]() mutable {
            brls::Application::getStyle().addMetric(
                "brls/animations/highlight", savedHighlight);
            // 保持旧菜单语义：根页面先收到关闭通知，再执行业务监听。
            invokeCallback(onDismiss);
            invokeCallback(afterClose);
        });
}

void ContextMenu::acquireDrawerInputBlock() {
    if (m_drawerInputBlocked) return;
    brls::Application::blockInputs();
    m_drawerInputBlocked = true;
}

void ContextMenu::releaseDrawerInputBlock() {
    if (!m_drawerInputBlocked) return;
    m_drawerInputBlocked = false;
    brls::Application::unblockInputs();
}

void ContextMenu::startDrawerOpen() {
    if (m_drawerAnimation.isRunning()) return;

    m_drawerDistance = getWidth();
    m_panel->setTranslationX(m_drawerDistance);
    m_hintCard->setAlpha(0.0f);

    acquireDrawerInputBlock();
    std::weak_ptr<Session> viewSession = m_viewSession;
    auto* menu = this;
    m_drawerDone = [viewSession, menu] {
        if (viewSession.expired()) return;
        menu->releaseDrawerInputBlock();
    };

    m_drawerAnimation.reset(0.0f);
    m_drawerAnimation.addStep(1.0f, DRAWER_OPEN_MS, brls::EasingFunction::cubicOut);
    m_drawerAnimation.start();
}

void ContextMenu::startDrawerClose(std::function<void()> afterClose) {
    m_drawerDistance = getWidth();
    acquireDrawerInputBlock();

    std::weak_ptr<Session> viewSession = m_viewSession;
    auto* menu = this;
    m_drawerDone = [viewSession, menu, afterClose = std::move(afterClose)]() mutable {
        brls::sync([viewSession, menu, afterClose = std::move(afterClose)]() mutable {
            auto activeView = viewSession.lock();
            if (!activeView) return;
            // View 可能在实际 pop 前离场，不让检查用强引用延长其逻辑生命。
            activeView.reset();
            menu->releaseDrawerInputBlock();
            menu->popMenuActivity(std::move(afterClose));
        });
    };

    m_drawerAnimation.reset(1.0f);
    m_drawerAnimation.addStep(0.0f, DRAWER_CLOSE_MS, brls::EasingFunction::quadraticIn);
    m_drawerAnimation.start();
}

void ContextMenu::updateDrawerViews() {
    float progress = m_drawerAnimation.getValue();
    m_panel->setTranslationX(m_drawerDistance * (1.0f - progress));
    m_hintCard->setAlpha(std::clamp((progress - 0.25f) / 0.5f, 0.0f, 1.0f));
}

void ContextMenu::finishDrawer(bool finished) {
    updateDrawerViews();
    if (!finished || !m_drawerDone) return;

    auto done = std::move(m_drawerDone);
    m_drawerDone = nullptr;
    done();
}
