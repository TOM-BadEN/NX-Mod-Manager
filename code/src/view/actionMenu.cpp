/**
 * ActionMenu - 右侧菜单
 *
 * ── 创建菜单 ──
 *
 *   MenuPageConfig menu;
 *   menu.title = "菜单标题";
 *   auto& item = menu.addItem("选项名", "提示词");
 *   item.setAction([]{ ... });
 *   menu.show();
 *
 * ── 选项配置 ──
 *
 *   .setTitle("文字")        覆盖标题（静态）
 *   .setTitle(func)          覆盖标题（动态，每次显示时调用）
 *   .setAction(func)         点击回调（同步）
 *   .setAction(func<any>)    点击回调（异步，接收 asyncAction 返回值）
 *   .setAsyncAction(func)    异步任务（子线程，返回 std::any）
 *   .setBadge("文字")        右侧标签（静态）
 *   .setBadge(func)          右侧标签（动态，每次显示时调用）
 *   .setBadgeHighlight(func) 标签高亮条件（true=高亮色）
 *   .setSubmenu(&subPage)    点击进入子菜单（与 action 互斥）
 *   .setPopPage()            执行 action 后返回上级（用于子菜单选项）
 *   .setStayOpen()           执行 action 后留在当前页（用于开关类选项）
 *   .setDisabled(func)       动态禁用条件（true=禁用）
 *
 * ── 页面配置 ──
 *
 *   menu.defaultFocus = []{ return index; };     打开时聚焦第几项
 *   menu.onDismiss = []{ ... };                  菜单关闭时回调
 *
 * ── 多选模式 ──
 *
 *   menu.multiSelect = true;
 *   menu.onConfirm = [](const vector<int>& selected){ ... };  // +键提交
 *
 * ── 示例 ──
 *
 *   // 开关选项（留在当前页，badge 动态显示状态）
 *   auto& wifi = menu.addItem("Wi-Fi", "");
 *   wifi.setBadge([&]{ return wifiOn ? "开启" : "关闭"; });
 *   wifi.setBadgeHighlight([&]{ return wifiOn; });
 *   wifi.setAction([&]{ wifiOn = !wifiOn; });
 *   wifi.setStayOpen();
 *
 *   // 子菜单入口
 *   auto& langItem = menu.addItem("语言", "");
 *   langItem.setBadge([&]{ return langNames[lang]; });
 *   langItem.setSubmenu(&langMenu);
 */

#include "view/actionMenu.hpp"
#include <borealis/core/i18n.hpp>
#include "view/actionMenuItem.hpp"
#include "core/audio.hpp"
#include "utils/pageNav.hpp"

// ── MenuPageConfig::show() ──────────────────────────────────────

void MenuPageConfig::show() {
    // 假焦点框：回调返回 false 或未设置时默认绘制
    bool showFake = !shouldShowFakeHighlight || shouldShowFakeHighlight();
    auto* prevFocus = showFake ? brls::Application::getCurrentFocus() : nullptr;

    auto* actionMenu = new ActionMenu(this, prevFocus);
    auto* activity = new brls::Activity(actionMenu);
    pageNav::push(activity);
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
        if (m_page->multiSelect) cell->setItem(item.getTitle(), m_menu->isSelected(index) ? "\uE876" : "");
        else cell->setItem(item.getTitle(), item.getBadge());
        cell->setMultiSelectLayout(m_page->multiSelect);
        cell->setDisabled(item.isDisabled());
        if (item.badgeHighlightCondition)
            cell->setBadgeHighlighted(item.badgeHighlightCondition());
        // 最后一项不显示分隔线
        cell->setLineBottom(index < m_page->items.size() - 1 ? 1 : 0);
        return cell;
    }

    void onItemSelected(RecyclingGrid* grid, size_t index) override {
        if (m_menu->isAsyncRunning()) return;

        auto& item = m_page->items[index];
        if (item.isDisabled()) return;

        // 异步 action（优先于普通 action 和 submenu）
        if (item.asyncAction) {
            m_menu->startAsync(index, item);
            return;
        }

        // 多选模式：翻转勾选
        if (m_page->multiSelect) {
            m_menu->toggleSelected(index);
            // 更新当前 Cell（11.1: 不要 reloadData）
            auto* cell = dynamic_cast<ActionMenuItem*>(grid->getGridItemByIndex(index));
            if (cell) {
                cell->setItem(item.getTitle(), m_menu->isSelected(index) ? "\uE876" : "");
            }
            return;
        }

        // 子菜单
        if (item.submenu) {
            m_menu->pushPage(item.submenu);
            return; // pushPage 内 setDataSource 已 delete 当前 DS，此后禁止访问成员
        }

        // AfterAction 处理
        m_menu->executeAfterAction(item.afterAction, item, index);
    }

    void clearData() override {}

private:
    ActionMenu* m_menu;
    MenuPageConfig* m_page;
};

// ── ActionMenu 构造函数 ─────────────────────────────────────────

ActionMenu::~ActionMenu() {
    m_alive->store(false);
}

ActionMenu::ActionMenu(MenuPageConfig* rootPage, brls::View* prevFocus)
    : m_rootPage(rootPage), m_prevFocus(prevFocus)
{
    // 沿父链查找最近的 ScrollingFrame，用于假焦点框裁切
    auto* p = m_prevFocus ? m_prevFocus->getParent() : nullptr;
    while (p && !dynamic_cast<brls::ScrollingFrame*>(p)) p = p->getParent();
    m_clipView = p;

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
    registerAction(brls::getStr("views/actionMenu/back"), brls::ControllerButton::BUTTON_B, [this](brls::View*) {
        Audio::instance()->play(SoundEffect::Enter);
        if (m_asyncRunning) {
            m_asyncTask.request_stop();
            stopAsync();
            return true;
        }
        popPage();
        return true;
    });

    // 异步期间拦截上下导航（永久注册，由 m_asyncRunning 控制是否吞掉事件）
    registerAction("", brls::BUTTON_NAV_UP, [this](brls::View*) {
        if (!m_asyncRunning) return false;
        auto* f = brls::Application::getCurrentFocus();
        if (f) f->shakeHighlight(brls::FocusDirection::UP);
        return true;
    }, true, true);
    registerAction("", brls::BUTTON_NAV_DOWN, [this](brls::View*) {
        if (!m_asyncRunning) return false;
        auto* f = brls::Application::getCurrentFocus();
        if (f) f->shakeHighlight(brls::FocusDirection::DOWN);
        return true;
    }, true, true);

    // X/+ 键仅多选模式注册（由 pushPage 中 updateMultiSelectActions 控制）

    // 默认隐藏提示词卡片（pushPage 延迟到 willAppear，确保 RecyclingGrid 已完成布局）
    m_hintCard->setVisibility(brls::Visibility::INVISIBLE);

    // 触摸左侧空白区域关闭菜单（排除提示词卡片）
    m_left->addGestureRecognizer(new brls::TapGestureRecognizer([this](brls::TapGestureStatus status, brls::Sound* sound) {
        if (status.state != brls::GestureState::END) return;
        if (m_asyncRunning) {
            m_asyncTask.request_stop();
            stopAsync();
            return;
        }
        if (m_hintCard->getVisibility() == brls::Visibility::VISIBLE && m_hintCard->getFrame().pointInside(status.position)) return;
        Audio::instance()->play(SoundEffect::Enter);
        closeMenu();
    }));
}

// 菜单打开时，底层焦点控件失去焦点高亮。在此绘制假焦点框（复刻框架 drawHighlight 边框部分），
// 让用户能看到菜单是从哪个控件上打开的。高亮背景无法复刻（需插入控件内部层级），仅绘制边框。
void ActionMenu::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) {
    if (m_prevFocus && brls::Application::getInputType() != brls::InputType::TOUCH) {
        // 裁切到 ScrollingFrame 可见区域，防止假焦点框溢出滚动容器边界
        if (m_clipView) {
            nvgSave(vg);
            nvgIntersectScissor(vg, m_clipView->getX(), m_clipView->getY(),
                m_clipView->getWidth(), m_clipView->getHeight());
        }

        float sw = style["brls/highlight/stroke_width"];
        float cr = style["brls/highlight/corner_radius"];
        brls::Theme theme = brls::Application::getTheme();
        float fx = m_prevFocus->getX() - sw / 2;
        float fy = m_prevFocus->getY() - sw / 2;
        float fw = m_prevFocus->getWidth() + sw;
        float fh = m_prevFocus->getHeight() + sw;

        // 外阴影
        float shadowOffset = style["brls/highlight/shadow_offset"];
        NVGpaint shadowPaint = nvgBoxGradient(vg,
            fx, fy + style["brls/highlight/shadow_width"],
            fw, fh,
            cr * 2, style["brls/highlight/shadow_feather"],
            nvgRGBA(0, 0, 0, (int)style["brls/highlight/shadow_opacity"]), brls::TRANSPARENT);
        nvgBeginPath(vg);
        nvgRect(vg, fx - shadowOffset, fy - shadowOffset,
            fw + shadowOffset * 2, fh + shadowOffset * 3);
        nvgRoundedRect(vg, fx, fy, fw, fh, cr);
        nvgPathWinding(vg, NVG_HOLE);
        nvgFillPaint(vg, shadowPaint);
        nvgFill(vg);

        // 脉冲边框
        float gradientX, gradientY, colorAnim;
        brls::getHighlightAnimation(&gradientX, &gradientY, &colorAnim);

        NVGcolor color1 = theme["brls/highlight/color1"];
        NVGcolor pulsationColor = nvgRGBAf(
            colorAnim * color1.r + (1 - colorAnim) * color1.r,
            colorAnim * color1.g + (1 - colorAnim) * color1.g,
            colorAnim * color1.b + (1 - colorAnim) * color1.b, 1.0f);

        nvgBeginPath(vg);
        nvgStrokeColor(vg, pulsationColor);
        nvgStrokeWidth(vg, sw);
        nvgRoundedRect(vg, fx, fy, fw, fh, cr);
        nvgStroke(vg);

        // 光点 1
        NVGcolor borderColor = theme["brls/highlight/color2"];
        borderColor.a = 0.5f;
        NVGpaint border1 = nvgRadialGradient(vg,
            fx + gradientX * fw, fy + gradientY * fh,
            sw * 10, sw * 40, borderColor, brls::TRANSPARENT);
        nvgBeginPath(vg);
        nvgStrokePaint(vg, border1);
        nvgStrokeWidth(vg, sw);
        nvgRoundedRect(vg, fx, fy, fw, fh, cr);
        nvgStroke(vg);

        // 光点 2
        NVGpaint border2 = nvgRadialGradient(vg,
            fx + (1 - gradientX) * fw, fy + (1 - gradientY) * fh,
            sw * 10, sw * 40, borderColor, brls::TRANSPARENT);
        nvgBeginPath(vg);
        nvgStrokePaint(vg, border2);
        nvgStrokeWidth(vg, sw);
        nvgRoundedRect(vg, fx, fy, fw, fh, cr);
        nvgStroke(vg);

        if (m_clipView) nvgRestore(vg);
    }
    Box::draw(vg, x, y, width, height, style, ctx);
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

    m_menuStack.push_back({page, 0, 0.0f, {}});
    m_selectedCount = 0;
    if (page->multiSelect) {
        m_menuStack.back().selectedStates.resize(page->items.size(), false);
    }

    // 多选模式加宽面板，容纳更多底栏按钮提示
    m_panel->setWidthPercentage(page->multiSelect ? 40.0f : 37.0f);

    // 更新标题（超长时自动滚动）
    m_title->setText(page->title);
    m_title->setAnimated(true);

    // 动态注册/注销多选按键
    updateMultiSelectActions(page->multiSelect);

    // 设置数据源（RecyclingGrid 会 delete 旧 DS）
    size_t focusIndex = page->defaultFocus ? page->defaultFocus() : 0;
    m_grid->setDefaultCellFocus(focusIndex);
    m_grid->setDataSource(new ActionMenuItemDS(this, page));
    m_grid->instantFocus(focusIndex);
}

void ActionMenu::popPage() {
    if (m_menuStack.size() <= 1) {
        // 栈空 → 关闭菜单
        closeMenu();
        return;
    }

    m_menuStack.pop_back();
    auto& parent = m_menuStack.back();

    // 更新标题（超长时自动滚动）
    m_title->setText(parent.page->title);
    m_title->setAnimated(true);

    // 动态注册/注销多选按键
    updateMultiSelectActions(parent.page->multiSelect);

    // 恢复焦点 + reloadData 刷新 badge（先设数据源，再改宽度，避免布局重算时旧 DS 野指针）
    m_grid->setDefaultCellFocus(parent.focusIndex);
    m_grid->setDataSource(new ActionMenuItemDS(this, parent.page));
    m_panel->setWidthPercentage(parent.page->multiSelect ? 40.0f : 37.0f);
    m_grid->setContentOffsetY(parent.scrollOffset, false);
    m_grid->instantFocus(parent.focusIndex);
}

void ActionMenu::closeMenu() {
    // 11.4/11.9: 任何关闭方式都触发根菜单 onDismiss
    auto onDismiss = m_rootPage->onDismiss;
    // 瞬移焦点动画，避免假→真脉冲闪烁
    auto style = brls::Application::getStyle();
    float saved = style["brls/animations/highlight"];
    style.addMetric("brls/animations/highlight", 1.0f);
    brls::Application::popActivity(brls::TransitionAnimation::NONE, [onDismiss, saved]() {
        brls::Application::getStyle().addMetric("brls/animations/highlight", saved);
        if (onDismiss) onDismiss();
    });
}

// ── 多选按键动态注册 ─────────────────────────────────────────────

bool ActionMenu::isSelected(size_t index) const {
    return m_menuStack.back().selectedStates[index];
}

void ActionMenu::toggleSelected(size_t index) {
    auto& states = m_menuStack.back().selectedStates;
    states[index] = !states[index];
    if (states[index]) m_selectedCount++;
    else m_selectedCount--;
    setActionAvailable(brls::ControllerButton::BUTTON_START, m_selectedCount > 0);
}

void ActionMenu::updateMultiSelectActions(bool multiSelect) {
    // 注销旧的 + action
    if (m_actionStart != ACTION_NONE) { unregisterAction(m_actionStart); m_actionStart = ACTION_NONE; }

    if (!multiSelect) return;

    // 多选模式：注册 + 键提交
    m_actionStart = registerAction(brls::getStr("views/actionMenu/submit"), brls::ControllerButton::BUTTON_START, [this](brls::View*) {
        auto* currentPage = m_menuStack.back().page;
        if (!currentPage->onConfirm) return true;
        Audio::instance()->play(SoundEffect::Enter);

        std::vector<int> indices;
        for (size_t i = 0; i < currentPage->items.size(); i++) {
            if (m_menuStack.back().selectedStates[i]) indices.push_back(static_cast<int>(i));
        }
        auto onConfirm = currentPage->onConfirm;
        closeMenu();
        onConfirm(indices);
        return true;
    });
    setActionAvailable(brls::ControllerButton::BUTTON_START, m_selectedCount > 0);
}

// ── 提示词卡片 + 索引更新 ───────────────────────────────────────

void ActionMenu::updateHint(size_t index) {
    auto* currentPage = m_menuStack.back().page;
    if (index >= currentPage->items.size()) return;

    const auto& item = currentPage->items[index];
    if (item.hint.empty()) {
        m_hintCard->setVisibility(brls::Visibility::INVISIBLE);
    } else {
        m_hintTitle->setText(item.getTitle());
        m_hint->setText(item.hint);
        m_hintCard->setVisibility(brls::Visibility::VISIBLE);
    }
}

void ActionMenu::updateIndex(size_t index) {
    auto* currentPage = m_menuStack.back().page;
    m_index->setText(std::to_string(index + 1) + "/" + std::to_string(currentPage->items.size()));
}

// ── 异步操作 ───────────────────────────────────────────────────

bool ActionMenu::isAsyncRunning() const { return m_asyncRunning; }

void ActionMenu::startAsync(size_t index, const MenuItemConfig& item) {
    m_asyncRunning = true;
    m_asyncIndex = index;
    m_animFrame = 0;

    // 启动异步任务（util::async 自动传 stop_token 给 lambda）
    m_asyncTask = util::async(item.asyncAction);

    // 启动 brls::sync 自循环轮询（动画 + 完成检测 + afterAction，全部在 performSyncTasks 第4步执行）
    pollAsync();
}

void ActionMenu::pollAsync() {
    auto alive = m_alive;
    brls::sync([this, alive] {
        if (!alive->load()) return;  // 对象已销毁，终止递归
        if (!m_asyncRunning) return;  // 已取消，不再轮询

        // 动画更新
        static const std::string frames[] = {"\uE020", "\uE021", "\uE022", "\uE023", "\uE024", "\uE025", "\uE026", "\uE027"};
        if (++m_animFrame % 10 == 0) {
            auto* cell = m_grid->getGridItemByIndex(m_asyncIndex);
            if (cell) static_cast<ActionMenuItem*>(cell)->setItem(
                m_menuStack.back().page->items[m_asyncIndex].getTitle(),
                frames[(m_animFrame / 10) % std::size(frames)]);
        }

        // 任务完成检测（不阻塞）
        if (m_asyncTask.valid() &&
            m_asyncTask.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            auto& item = m_menuStack.back().page->items[m_asyncIndex];
            auto result = m_asyncTask.get();
            stopAsync();
            executeAfterAction(item.afterAction, item, m_asyncIndex, std::move(result));
            return;  // 任务完成，不再轮询
        }

        // 未完成，下一帧继续轮询
        pollAsync();
    });
}

void ActionMenu::stopAsync() {
    // 恢复 badge 原始文字
    auto* cell = m_grid->getGridItemByIndex(m_asyncIndex);
    auto& items = m_menuStack.back().page->items;
    if (cell) {
        static_cast<ActionMenuItem*>(cell)->setItem(
            items[m_asyncIndex].getTitle(), items[m_asyncIndex].getBadge());
    }

    m_asyncRunning = false;
    m_animFrame = 0;
}

void ActionMenu::executeAfterAction(AfterAction afterAction, const MenuItemConfig& item, size_t index, std::any result) {
    // 执行回调：异步优先用 asyncComplete（带结果），否则 fallback 到 action
    auto invokeAction = [&] {
        if (item.asyncComplete) item.asyncComplete(std::move(result));
        else if (item.action) item.action();
    };

    switch (afterAction) {
        case AfterAction::CloseMenu:
            closeMenu();
            invokeAction();
            break;
        case AfterAction::PopPage:
            invokeAction();
            popPage();
            break;
        case AfterAction::Stay: {
            invokeAction();
            auto& items = m_menuStack.back().page->items;
            for (size_t i = 0; i < items.size(); i++) {
                auto* cell = dynamic_cast<ActionMenuItem*>(m_grid->getGridItemByIndex(i));
                if (!cell) continue;
                cell->setItem(items[i].getTitle(), items[i].getBadge());
                if (items[i].badgeHighlightCondition) cell->setBadgeHighlighted(items[i].badgeHighlightCondition());
            }
            break;
        }
    }
}
