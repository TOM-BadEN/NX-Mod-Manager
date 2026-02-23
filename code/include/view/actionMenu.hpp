/**
 * ActionMenu - 通用右侧弹出菜单组件
 * 支持单选、多级菜单、多选三种模式
 */

#pragma once

#include <borealis.hpp>
#include <string>
#include <vector>
#include <functional>
#include "view/recyclingGrid.hpp"

struct MenuPageConfig;

enum class AfterAction {
    CloseMenu,  // 退出整个菜单（默认）
    PopPage,    // 返回上一级（子菜单选择场景，栈空则自动关闭）
    Stay,       // 留在当前页（开关类选项）
};

struct MenuItemConfig {
    // 基础字段
    std::string titleText;                                  // 静态标题
    std::string hintText;                                   // 提示词卡片文字
    std::string badgeText;                                  // 静态 badge（右侧小字）
    std::function<void()> onAction              = nullptr;  // 选中回调
    MenuPageConfig* subMenuPtr                  = nullptr;  // 子菜单（与 onAction 互斥）
    bool enabled                                = true;     // 是否可选
    bool selected                               = false;    // 多选勾选（ActionMenu 内部管理）
    AfterAction afterAction                     = AfterAction::CloseMenu;

    // 动态覆盖（可选，运行时优先于静态值）
    std::function<std::string()> titleGetter    = nullptr;  // 动态标题
    std::function<std::string()> badgeGetter    = nullptr;  // 动态 badge
    std::function<bool()> badgeHighlightGetter  = nullptr;  // badge 是否高亮（nullptr=始终高亮）

    // 链式方法
    MenuItemConfig& title(const std::string& s);            // 设置静态标题
    MenuItemConfig& title(std::function<std::string()> f);  // 设置动态标题
    MenuItemConfig& badge(const std::string& s);            // 设置静态 badge
    MenuItemConfig& badge(std::function<std::string()> f);  // 设置动态 badge
    MenuItemConfig& action(std::function<void()> a);        // 设置选中回调
    MenuItemConfig& submenu(MenuPageConfig* s);             // 设置子菜单
    MenuItemConfig& disabled();                             // 标记不可选
    MenuItemConfig& popPage();                              // action 后返回上级
    MenuItemConfig& stayOpen();                             // action 后留在当前页
    MenuItemConfig& badgeHighlight(std::function<bool()> f); // badge 动态高亮

    // ActionMenu 内部取值（优先动态，回退静态）
    std::string getTitle() const;
    std::string getBadge() const;
};

struct MenuPageConfig {
    std::string title;                                                      // 页面标题
    std::vector<MenuItemConfig> items;                                      // 选项列表
    bool multiSelect = false;                                               // 多选模式
    std::function<void(const std::vector<int>&)> onConfirm = nullptr;       // 多选确认回调
    std::function<void()> onDismiss = nullptr;                              // 菜单关闭回调
    std::function<size_t()> defaultFocus = nullptr;                           // 动态初始焦点索引

    void show();  // 弹出菜单（内部创建 ActionMenu + pushActivity）
};

struct MenuStackEntry {
    MenuPageConfig* page;   // 当前页配置
    size_t focusIndex;      // 离开时的焦点位置（返回时恢复）
    float scrollOffset;     // 离开时的滚动位置（返回时恢复）
};

class ActionMenu : public brls::Box {
public:
    ActionMenu(MenuPageConfig* rootPage, brls::View* prevFocus = nullptr);

    bool isTranslucent() override { return true || View::isTranslucent(); }
    void willAppear(bool resetState) override;
    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) override;

    void pushPage(MenuPageConfig* page);        // 进入子菜单
    void popPage();                             // 返回上级（栈空则关闭菜单）
    void closeMenu();                           // 关闭整个菜单 + popActivity

private:
    MenuPageConfig* m_rootPage;                 // 根菜单页（由调用方持有生命周期）
    brls::View* m_prevFocus = nullptr;           // 菜单打开前的焦点（用于画假焦点框）
    std::vector<MenuStackEntry> m_menuStack;    // 多级菜单栈

    brls::ActionIdentifier m_actionStart = ACTION_NONE;  // + 键（提交）action ID

    void updateHint(size_t index);              // 更新左侧提示词卡片
    void updateIndex(size_t index);             // 更新底部索引（如 "2/4"）
    void updateMultiSelectActions(bool multiSelect);  // 动态注册/注销多选按键

    BRLS_BIND(brls::Label, m_title, "actionMenu/title");        // Header 标题
    BRLS_BIND(brls::Label, m_version, "actionMenu/version");    // Header 版本号
    BRLS_BIND(RecyclingGrid, m_grid, "actionMenu/grid");        // 选项列表
    BRLS_BIND(brls::Label, m_index, "actionMenu/index");        // Footer 索引
    BRLS_BIND(brls::Label, m_hintTitle, "actionMenu/hintTitle"); // 提示词标题
    BRLS_BIND(brls::Label, m_hint, "actionMenu/hint");          // 提示词内容
    BRLS_BIND(brls::Box, m_hintCard, "actionMenu/hintCard");    // 提示词卡片容器
    BRLS_BIND(brls::Box, m_left, "actionMenu/left");              // 左侧区域（触摸关闭）
};
