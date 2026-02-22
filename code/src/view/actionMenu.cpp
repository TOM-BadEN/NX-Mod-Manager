/**
 * ActionMenu - 通用右侧弹出菜单组件实现
 * 包含：链式方法、ActionMenuItemDS（内部数据源）、ActionMenu 核心逻辑
 */

#include "view/actionMenu.hpp"
#include "view/actionMenuItem.hpp"

// ── MenuItemConfig 链式方法 ─────────────────────────────────────

MenuItemConfig& MenuItemConfig::title(const std::string& s) { titleText = s; return *this; }
MenuItemConfig& MenuItemConfig::title(std::function<std::string()> f) { titleGetter = std::move(f); return *this; }
MenuItemConfig& MenuItemConfig::badge(const std::string& s) { badgeText = s; return *this; }
MenuItemConfig& MenuItemConfig::badge(std::function<std::string()> f) { badgeGetter = std::move(f); return *this; }
MenuItemConfig& MenuItemConfig::action(std::function<void()> a) { onAction = std::move(a); return *this; }
MenuItemConfig& MenuItemConfig::submenu(MenuPageConfig* s) { subMenuPtr = s; return *this; }
MenuItemConfig& MenuItemConfig::disabled() { enabled = false; return *this; }
MenuItemConfig& MenuItemConfig::popPage() { afterAction = AfterAction::PopPage; return *this; }
MenuItemConfig& MenuItemConfig::stayOpen() { afterAction = AfterAction::Stay; return *this; }
MenuItemConfig& MenuItemConfig::badgeHighlight(std::function<bool()> f) { badgeHighlightGetter = std::move(f); return *this; }

std::string MenuItemConfig::getTitle() const { return titleGetter ? titleGetter() : titleText; }
std::string MenuItemConfig::getBadge() const { return badgeGetter ? badgeGetter() : badgeText; }

// ── MenuPageConfig::show() ──────────────────────────────────────

void MenuPageConfig::show() {
    // 多选模式自动重置 selected
    if (multiSelect) {
        for (auto& item : items) item.selected = false;
    }

    auto* actionMenu = new ActionMenu(this);
    auto* activity = new brls::Activity(actionMenu);
    brls::Application::pushActivity(activity, brls::TransitionAnimation::NONE);
}

// ── ActionMenuItemDS（内部数据源）────────────────────────────────

class ActionMenuItemDS : public RecyclingGridDataSource {
public:
    ActionMenuItemDS(ActionMenu* menu, MenuPageConfig* page)
        : m_menu(menu), m_page(page) {}

    size_t getItemCount() override { return m_page->items.size(); }

    RecyclingGridItem* cellForRow(RecyclingGrid* grid, size_t index) override {
        auto* cell = static_cast<ActionMenuItem*>(grid->dequeueReusableCell("ActionMenuItem"));
        auto& item = m_page->items[index];
        if (m_page->multiSelect)
            cell->setItem(item.getTitle(), item.selected ? "\uE876" : "");
        else
            cell->setItem(item.getTitle(), item.getBadge());
        cell->setDisabled(!item.enabled);
        if (item.badgeHighlightGetter)
            cell->setBadgeHighlighted(item.badgeHighlightGetter());
        // 最后一项不显示分隔线
        cell->setLineBottom(index < m_page->items.size() - 1 ? 1 : 0);
        return cell;
    }

    void onItemSelected(RecyclingGrid* grid, size_t index) override {
        auto& item = m_page->items[index];
        if (!item.enabled) return;

        // 多选模式：翻转勾选
        if (m_page->multiSelect) {
            item.selected = !item.selected;
            // 更新当前 Cell（11.1: 不要 reloadData）
            auto* cell = dynamic_cast<ActionMenuItem*>(grid->getGridItemByIndex(index));
            if (cell) {
                cell->setItem(item.getTitle(), item.selected ? "\uE876" : "");
            }
            return;
        }

        // 子菜单
        if (item.subMenuPtr) {
            m_menu->pushPage(item.subMenuPtr);
            return; // pushPage 内 setDataSource 已 delete 当前 DS，此后禁止访问成员
        }

        // AfterAction 处理
        switch (item.afterAction) {
            case AfterAction::CloseMenu: {
                // 先关菜单再执行 action，避免 action 中 pushActivity 与 popActivity 栈顺序冲突
                m_menu->closeMenu();
                // closeMenu 后 DS 已 delete，但 item 引用外部 MenuPageConfig 数据（仍存活）
                if (item.onAction) item.onAction();
                break;
            }
            case AfterAction::PopPage:
                if (item.onAction) item.onAction();
                m_menu->popPage();
                break; // popPage 内 setDataSource 已 delete 当前 DS，此后禁止访问成员
            case AfterAction::Stay: {
                if (item.onAction) item.onAction();
                // 直接更新当前 Cell
                auto* cell = dynamic_cast<ActionMenuItem*>(grid->getGridItemByIndex(index));
                if (cell) {
                    cell->setItem(item.getTitle(), item.getBadge());
                    if (item.badgeHighlightGetter) cell->setBadgeHighlighted(item.badgeHighlightGetter());
                }
                break;
            }
        }
    }

    void clearData() override {}

private:
    ActionMenu* m_menu;
    MenuPageConfig* m_page;
};

// ── ActionMenu 构造函数 ─────────────────────────────────────────

ActionMenu::ActionMenu(MenuPageConfig* rootPage)
    : m_rootPage(rootPage)
{
    inflateFromXMLRes("xml/view/actionMenu.xml");

    // 版本号（APP_VERSION 宏由 CMakeLists.txt 定义）
    m_version->setText(APP_VERSION);

    // 列表内边距（与 ModList 一致，统一由 RecyclingGrid 内部管理）
    m_grid->setPadding(30, 35, 30, 28);

    // 注册 Cell 类型
    m_grid->registerCell("ActionMenuItem", ActionMenuItem::create);

    // 焦点变化 → 更新提示词卡片 + 索引
    m_grid->setFocusChangeCallback([this](size_t index) {
        updateHint(index);
        updateIndex(index);
    });

    // 注册按键
    // B 键：返回上级 / 关闭（覆盖默认行为）
    registerAction("返回", brls::ControllerButton::BUTTON_B, [this](brls::View*) {
        popPage();
        return true;
    });

    // X/+ 键仅多选模式注册（由 pushPage 中 updateMultiSelectActions 控制）

    // 默认隐藏提示词卡片（pushPage 延迟到 willAppear，确保 RecyclingGrid 已完成布局）
    m_hintCard->setVisibility(brls::Visibility::INVISIBLE);
}

void ActionMenu::willAppear(bool resetState) {
    Box::willAppear(resetState);
    // 首次显示时加载初始页面（此时 resizeToFitWindow 已执行，RecyclingGrid 已布局）
    if (m_menuStack.empty()) {
        pushPage(m_rootPage);
    }
}

// ── 菜单栈操作 ──────────────────────────────────────────────────

void ActionMenu::pushPage(MenuPageConfig* page) {
    // 保存当前页焦点 + 滚动位置
    if (!m_menuStack.empty()) {
        m_menuStack.back().focusIndex = m_grid->getDefaultCellFocus();
        m_menuStack.back().scrollOffset = m_grid->getContentOffsetY();
    }

    m_menuStack.push_back({page, 0});

    // 更新标题
    m_title->setText(page->title);

    // 动态注册/注销多选按键
    updateMultiSelectActions(page->multiSelect);

    // 多选模式：重置勾选状态
    if (page->multiSelect) {
        for (auto& item : page->items) item.selected = false;
    }

    // 转移焦点到非 Cell 视图，避免 setDataSource 回收时 LIFO 碰撞导致 giveFocus 跳过
    m_title->setFocusable(true);
    brls::Application::giveFocus(m_title);
    m_title->setFocusable(false);

    // 设置数据源（RecyclingGrid 会 delete 旧 DS）
    size_t focusIndex = page->defaultFocus ? page->defaultFocus() : 0;
    m_grid->setDefaultCellFocus(focusIndex);
    m_grid->setDataSource(new ActionMenuItemDS(this, page));
    brls::Application::giveFocus(m_grid);
    // 抑制 handleAction 残留点击动画（旧 Cell 被 LIFO 复用为焦点行）
    if (auto* cell = m_grid->getGridItemByIndex(focusIndex)) {
        cell->setHideClickAnimation(true);
    }
}

void ActionMenu::popPage() {
    if (m_menuStack.size() <= 1) {
        // 栈空 → 关闭菜单
        closeMenu();
        return;
    }

    m_menuStack.pop_back();
    auto& parent = m_menuStack.back();

    // 更新标题
    m_title->setText(parent.page->title);

    // 动态注册/注销多选按键
    updateMultiSelectActions(parent.page->multiSelect);

    // 转移焦点到非 Cell 视图，避免 setDataSource 回收时 LIFO 碰撞导致 giveFocus 跳过
    m_title->setFocusable(true);
    brls::Application::giveFocus(m_title);
    m_title->setFocusable(false);

    // 11.6: 恢复焦点 + reloadData 刷新 badge
    m_grid->setDefaultCellFocus(parent.focusIndex);
    m_grid->setDataSource(new ActionMenuItemDS(this, parent.page));
    m_grid->setContentOffsetY(parent.scrollOffset, false);
    brls::Application::giveFocus(m_grid);
    // 抑制 handleAction 残留点击动画（旧 Cell 被 LIFO 复用为焦点行）
    if (auto* cell = m_grid->getGridItemByIndex(parent.focusIndex)) {
        cell->setHideClickAnimation(true);
    }
}

void ActionMenu::closeMenu() {
    // 11.4/11.9: 任何关闭方式都触发根菜单 onDismiss
    auto onDismiss = m_rootPage->onDismiss;
    brls::Application::popActivity(brls::TransitionAnimation::NONE, [onDismiss]() {
        if (onDismiss) onDismiss();
    });
}

// ── 多选按键动态注册 ─────────────────────────────────────────────

void ActionMenu::updateMultiSelectActions(bool multiSelect) {
    // 注销旧的 + action
    if (m_actionStart != ACTION_NONE) { unregisterAction(m_actionStart); m_actionStart = ACTION_NONE; }

    if (!multiSelect) return;

    // 多选模式：注册 + 键提交
    m_actionStart = registerAction("提交", brls::ControllerButton::BUTTON_START, [this](brls::View*) {
        auto* currentPage = m_menuStack.back().page;
        if (!currentPage->onConfirm) return true;

        std::vector<int> indices;
        for (size_t i = 0; i < currentPage->items.size(); i++) {
            if (currentPage->items[i].selected) indices.push_back(static_cast<int>(i));
        }
        auto onConfirm = currentPage->onConfirm;
        closeMenu();
        onConfirm(indices);
        return true;
    });
}

// ── 提示词卡片 + 索引更新 ───────────────────────────────────────

void ActionMenu::updateHint(size_t index) {
    auto* currentPage = m_menuStack.back().page;
    if (index >= currentPage->items.size()) return;

    const auto& item = currentPage->items[index];
    if (item.hintText.empty()) {
        m_hintCard->setVisibility(brls::Visibility::INVISIBLE);
    } else {
        m_hintTitle->setText(item.getTitle());
        m_hint->setText(item.hintText);
        m_hintCard->setVisibility(brls::Visibility::VISIBLE);
    }
}

void ActionMenu::updateIndex(size_t index) {
    auto* currentPage = m_menuStack.back().page;
    m_index->setText(std::to_string(index + 1) + "/" + std::to_string(currentPage->items.size()));
}
