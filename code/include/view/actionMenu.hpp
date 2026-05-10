/**
 * ActionMenu - 通用右侧弹出菜单组件
 * 支持单选、多级菜单、多选三种模式
 */

#pragma once

#include <borealis.hpp>
#include <string>
#include <vector>
#include <deque>
#include <functional>
#include <stop_token>
#include <memory>
#include <any>
#include "view/recyclingGrid.hpp"
#include "utils/async.hpp"

struct MenuPageConfig;

enum class AfterAction {
    CloseMenu,  // 退出整个菜单（默认）
    PopPage,    // 返回上一级（子菜单选择场景，栈空则自动关闭）
    Stay,       // 留在当前页（开关类选项）
};

struct MenuItemConfig {
    // 基础字段
    std::string title;                                        // 静态标题
    std::string hint;                                         // 提示词卡片文字
    std::string badge;                                        // 静态 badge（右侧小字）
    std::function<void()> action                  = nullptr;  // 选中回调（同步）
    std::function<void(std::any)> asyncComplete    = nullptr;  // 选中回调（异步，接收 asyncAction 返回值）
    std::function<std::any(std::stop_token)> asyncAction = nullptr;  // 异步任务（子线程，返回 std::any）
    MenuPageConfig* submenu                       = nullptr;  // 子菜单（与 action 互斥）
    std::function<bool()> disabledCondition        = nullptr;  // 动态禁用判断（nullptr=可选）
    AfterAction afterAction                        = AfterAction::CloseMenu;

    // 动态覆盖（可选，运行时优先于静态值）
    std::function<std::string()> dynamicTitle      = nullptr;  // 动态标题
    std::function<std::string()> dynamicBadge      = nullptr;  // 动态 badge
    std::function<bool()> badgeHighlightCondition  = nullptr;  // badge 是否高亮（nullptr=始终高亮）

    /**
     * @brief 构造菜单项
     * @param title 标题
     * @param hint 提示词
     */
    MenuItemConfig(const std::string& title, const std::string& hint = "")
        : title(title), hint(hint) {}

    /**
     * @brief 设置静态标题
     * @param s 标题文本
     */
    void setTitle(const std::string& s) {
        title = s;
    }

    /**
     * @brief 设置动态标题
     * @param f 动态标题回调
     */
    void setTitle(std::function<std::string()> f) {
        dynamicTitle = std::move(f);
    }

    /**
     * @brief 设置静态 badge
     * @param s badge 文本
     */
    void setBadge(const std::string& s) {
        badge = s;
    }

    /**
     * @brief 设置动态 badge
     * @param f 动态 badge 回调
     */
    void setBadge(std::function<std::string()> f) {
        dynamicBadge = std::move(f);
    }

    /**
     * @brief 设置同步回调
     * @param a 回调函数
     */
    void setAction(std::function<void()> a) {
        action = std::move(a);
    }

    /**
     * @brief 设置异步完成回调
     * @param a 回调函数（接收 asyncAction 返回值）
     */
    void setAction(std::function<void(std::any)> a) {
        asyncComplete = std::move(a);
    }

    /**
     * @brief 设置异步任务
     * @param a 异步任务函数
     */
    void setAsyncAction(std::function<std::any(std::stop_token)> a) {
        asyncAction = std::move(a);
    }

    /**
     * @brief 设置子菜单
     * @param s 子菜单配置
     */
    void setSubmenu(MenuPageConfig* s) {
        submenu = s;
    }

    /**
     * @brief 设置禁用条件
     * @param f 禁用判断回调
     */
    void setDisabled(std::function<bool()> f) {
        disabledCondition = std::move(f);
    }

    /** @brief 选中后返回上级 */
    void setPopPage() {
        afterAction = AfterAction::PopPage;
    }

    /** @brief 选中后保持菜单打开 */
    void setStayOpen() {
        afterAction = AfterAction::Stay;
    }

    /**
     * @brief 设置 badge 高亮条件
     * @param f 高亮判断回调
     */
    void setBadgeHighlight(std::function<bool()> f) {
        badgeHighlightCondition = std::move(f);
    }

    /** @brief 获取标题（优先动态，回退静态） */
    std::string getTitle() const {
        return dynamicTitle ? dynamicTitle() : title;
    }

    /** @brief 获取 badge（优先动态，回退静态） */
    std::string getBadge() const {
        return dynamicBadge ? dynamicBadge() : badge;
    }

    /** @brief 是否禁用 */
    bool isDisabled() const {
        return disabledCondition ? disabledCondition() : false;
    }
};

struct MenuPageConfig {
    std::string title;                                                      // 页面标题
    std::deque<MenuItemConfig> items;                                       // 选项列表
    bool multiSelect = false;                                               // 多选模式
    std::function<void(const std::vector<int>&)> onConfirm = nullptr;       // 多选确认回调
    std::function<void()> onDismiss = nullptr;                              // 菜单关闭回调
    std::function<size_t()> defaultFocus = nullptr;                         // 动态初始焦点索引
    std::function<bool()> shouldShowFakeHighlight = nullptr;                // 假焦点框开关（nullptr / 返回 true = 绘制）

    /**
     * @brief 添加菜单项
     * @param title 标题
     * @param hint 提示词
     */
    MenuItemConfig& addItem(const std::string& title, const std::string& hint = "") {
        items.emplace_back(title, hint);
        return items.back();
    }

    /** @brief 弹出菜单 */
    void show();
};

struct MenuStackEntry {
    MenuPageConfig* page;               // 当前页配置
    size_t focusIndex;                  // 离开时的焦点位置（返回时恢复）
    float scrollOffset;                 // 离开时的滚动位置（返回时恢复）
    std::vector<bool> selectedStates;   // 多选模式勾选状态（ActionMenu 内部管理）
};

class ActionMenu : public brls::Box {
public:
    /**
     * @brief 构造菜单
     * @param rootPage 根菜单页配置
     * @param prevFocus 菜单打开前的焦点
     */
    ActionMenu(MenuPageConfig* rootPage, brls::View* prevFocus = nullptr);
    ~ActionMenu() override;

    bool isTranslucent() override { return true || View::isTranslucent(); }
    void willAppear(bool resetState) override;
    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) override;

    /**
     * @brief 进入子菜单
     * @param page 子菜单配置
     */
    void pushPage(MenuPageConfig* page);

    /** @brief 返回上级（栈空则关闭菜单） */
    void popPage();

    /** @brief 关闭整个菜单 + popActivity */
    void closeMenu();

    /**
     * @brief 启动异步任务
     * @param index 选项索引
     * @param item 菜单项配置
     */
    void startAsync(size_t index, const MenuItemConfig& item);

    /** @brief 停止异步动画 + 恢复输入 */
    void stopAsync();

    /** @brief 是否有异步任务运行中 */
    bool isAsyncRunning() const;

    /**
     * @brief 查询多选状态
     * @param index 选项索引
     */
    bool isSelected(size_t index) const;

    /**
     * @brief 切换多选状态
     * @param index 选项索引
     */
    void toggleSelected(size_t index);

    /**
     * @brief 执行选中后动作
     * @param afterAction 动作类型
     * @param item 菜单项配置
     * @param index 选项索引
     * @param result 异步结果
     */
    void executeAfterAction(AfterAction afterAction, const MenuItemConfig& item, size_t index, std::any result = {});

private:
    std::shared_ptr<std::atomic<bool>> m_alive = std::make_shared<std::atomic<bool>>(true);  // 存活标记
    MenuPageConfig* m_rootPage;                 // 根菜单页（由调用方持有生命周期）
    brls::View* m_prevFocus = nullptr;           // 菜单打开前的焦点（用于画假焦点框）
    brls::View* m_clipView = nullptr;            // 假焦点裁切区域（最近的 ScrollingFrame）
    std::vector<MenuStackEntry> m_menuStack;    // 多级菜单栈

    brls::ActionIdentifier m_actionStart = ACTION_NONE;  // + 键（提交）action ID
    int m_selectedCount = 0;                                     // 当前多选页面已勾选数量

    bool m_asyncRunning = false;
    size_t m_asyncIndex = 0;
    int m_animFrame = 0;
    util::AsyncFurture<std::any> m_asyncTask;

    /** @brief 轮询异步任务状态（动画 + 完成检测 + afterAction） */
    void pollAsync();

    /**
     * @brief 更新左侧提示词卡片
     * @param index 选项索引
     */
    void updateHint(size_t index);

    /**
     * @brief 更新底部索引显示
     * @param index 选项索引
     */
    void updateIndex(size_t index);

    /**
     * @brief 动态注册/注销多选按键
     * @param multiSelect 是否多选模式
     */
    void updateMultiSelectActions(bool multiSelect);

    BRLS_BIND(brls::Label, m_title, "actionMenu/title");        // Header 标题
    BRLS_BIND(brls::Label, m_version, "actionMenu/version");    // Header 版本号
    BRLS_BIND(RecyclingGrid, m_grid, "actionMenu/grid");        // 选项列表
    BRLS_BIND(brls::Label, m_index, "actionMenu/index");        // Footer 索引
    BRLS_BIND(brls::Label, m_hintTitle, "actionMenu/hintTitle"); // 提示词标题
    BRLS_BIND(brls::Label, m_hint, "actionMenu/hint");          // 提示词内容
    BRLS_BIND(brls::Box, m_hintCard, "actionMenu/hintCard");    // 提示词卡片容器
    BRLS_BIND(brls::Box, m_left, "actionMenu/left");              // 左侧区域（触摸关闭）
    BRLS_BIND(brls::Box, m_panel, "actionMenu/panel");            // 右侧菜单面板
};
