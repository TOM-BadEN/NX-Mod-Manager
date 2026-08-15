/**
 * ContextMenu - 右侧上下文菜单
 *
 * ContextMenuPage 负责平铺创建菜单项，ContextMenu 负责显示、输入、
 * 页面栈以及后台任务的界面生命周期。
 */

#pragma once

#include "ui/view/contextMenuEntry.hpp"
#include "ui/view/recyclingGrid.hpp"
#include "utils/threadPool.hpp"
#include <borealis.hpp>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>

class ContextMenuCell;
class ContextMenuDataSource;
struct ContextMenuPageData;
struct ContextMultiSelectPageData;

/** @brief 普通上下文菜单页面 */
class ContextMenuPage {
public:
    /**
     * @brief 创建普通上下文菜单页面
     * @param title 页面标题
     */
    explicit ContextMenuPage(const std::string& title);

    /**
     * @brief 添加普通操作
     * @param title 菜单项标题
     * @param hint 菜单项提示文字
     * @return 新创建的普通操作菜单项
     */
    ContextMenuActionEntry& addAction(const std::string& title, const std::string& hint);

    /**
     * @brief 添加带类型结果的后台任务
     * @tparam Result 后台任务结果类型，void 表示无结果任务
     * @param title 菜单项标题
     * @param hint 菜单项提示文字
     * @return 新创建的普通后台任务菜单项
     */
    template <typename Result>
    ContextMenuTaskEntry<Result>& addTask(const std::string& title, const std::string& hint) {
        auto* task = new ContextMenuTaskEntry<Result>(title, hint);
        appendEntry(std::unique_ptr<ContextMenuEntry>(task));
        return *task;
    }

    /**
     * @brief 添加开关
     * @param title 菜单项标题
     * @param hint 菜单项提示文字
     * @return 新创建的开关菜单项
     */
    ContextMenuSwitchEntry& addSwitch(const std::string& title, const std::string& hint);

    /**
     * @brief 添加单选项
     * @param title 菜单项标题
     * @param hint 菜单项提示文字
     * @return 新创建的单选菜单项
     */
    ContextMenuRadioEntry& addRadio(const std::string& title, const std::string& hint);

    /**
     * @brief 添加子菜单入口
     * @param title 菜单项标题
     * @param hint 菜单项提示文字
     * @return 新创建的子菜单入口
     */
    ContextMenuSubmenuEntry& addSubmenu(const std::string& title, const std::string& hint);

    /**
     * @brief 设置页面标题图标
     * @param path 图标资源路径，空字符串表示不显示图标
     */
    void setIcon(const std::string& path);

    /**
     * @brief 设置根菜单关闭回调
     * @param onDismiss 根菜单关闭后执行的监听
     */
    void setOnDismiss(std::function<void()> onDismiss);

    /**
     * @brief 设置根菜单是否绘制打开前的假焦点框
     * @param show 返回是否绘制假焦点框的读取函数，返回 true 表示绘制
     */
    void setShowFakeHighlight(std::function<bool()> show);

    /** @brief 打开此页面作为根菜单 */
    void show() const;

private:
    friend class ContextMenu;
    friend class ContextMenuSubmenuEntry;

    /**
     * @brief 保存菜单项
     * @param entry 需要转移所有权的菜单项
     */
    void appendEntry(std::unique_ptr<ContextMenuEntry> entry);

    std::shared_ptr<ContextMenuPageData> m_data; // 页面共享数据
};

/** @brief 独立多选页面 */
class ContextMultiSelectPage {
public:
    /**
     * @brief 创建独立多选页面
     * @param title 页面标题
     */
    explicit ContextMultiSelectPage(const std::string& title);

    /**
     * @brief 添加多选项
     * @param title 选项标题
     * @param hint 选项提示文字
     * @return 新创建的多选项
     */
    ContextMultiSelectOption& addOption(const std::string& title, const std::string& hint);

    /**
     * @brief 设置多选结果提交监听
     * @param confirm 接收全部已选项索引的监听，索引顺序与选项添加顺序一致
     */
    void onConfirm(std::function<void(const std::vector<int>&)> confirm);

    /**
     * @brief 设置页面标题图标
     * @param path 图标资源路径，空字符串表示不显示图标
     */
    void setIcon(const std::string& path);

    /** @brief 打开多选页面 */
    void show() const;

private:
    friend class ContextMenu;

    std::shared_ptr<ContextMultiSelectPageData> m_data; // 多选页面共享数据
};

/** @brief 上下文菜单视图 */
class ContextMenu final : public brls::Box {
public:
    /** @brief 请求停止后台任务并销毁菜单 */
    ~ContextMenu() override;

    /**
     * @brief 查询菜单是否使用透明背景
     * @return 始终返回 true
     */
    bool isTranslucent() override { return true; }

    /**
     * @brief 首次显示时加载根页面并播放打开动画
     * @param resetState 是否重置视图状态
     */
    void willAppear(bool resetState) override;

    /**
     * @brief 离场时停止任务并释放输入锁
     * @param resetState 是否重置视图状态
     */
    void willDisappear(bool resetState) override;

    /**
     * @brief 绘制底层假焦点和菜单内容
     * @param vg NanoVG 绘图上下文
     * @param x 视图左上角横坐标
     * @param y 视图左上角纵坐标
     * @param width 视图宽度
     * @param height 视图高度
     * @param style 当前界面样式
     * @param ctx 当前帧上下文
     */
    void draw(NVGcontext* vg, float x, float y, float width, float height,
        brls::Style style, brls::FrameContext* ctx) override;

private:
    friend class ContextMenuDataSource;
    friend class ContextMenuPage;
    friend class ContextMultiSelectPage;

    class ActivityHost;

    static constexpr size_t NO_INDEX = static_cast<size_t>(-1);

    struct Session {};

    struct PageStackEntry {
        std::shared_ptr<ContextMenuPageData> page;
        size_t focusIndex = 0;
        float scrollOffset = 0.0f;
    };

    struct SwitchAnimation {
        size_t index;
        bool beforeState;
        bool afterState;
    };

    struct RadioAnimation {
        size_t index;
        bool beforeState;
    };

    struct FocusFrame {
        float x;
        float y;
        float width;
        float height;
    };

    enum class RunningKind {
        None,
        Task,
        Switch,
    };

    /**
     * @brief 创建普通上下文菜单
     * @param rootPage 普通根页面共享数据
     * @param previousFocus 菜单打开前的焦点视图，可为空
     */
    ContextMenu(std::shared_ptr<ContextMenuPageData> rootPage, brls::View* previousFocus);

    /**
     * @brief 创建独立多选菜单
     * @param multiPage 多选页面共享数据
     * @param previousFocus 菜单打开前的焦点视图，可为空
     */
    ContextMenu(std::shared_ptr<ContextMultiSelectPageData> multiPage, brls::View* previousFocus);

    /** @brief 加载布局并注册输入 */
    void initialize();

    /** @brief 终止当前菜单业务生命周期 */
    void invalidateSession();

    /** @brief 记录菜单 Activity 暂停 */
    void onActivityPause();

    /** @brief 记录菜单 Activity 恢复 */
    void onActivityResume();

    /**
     * @brief 查询菜单是否为当前栈顶 Activity
     * @return 菜单位于 Activity 栈顶时返回 true，否则返回 false
     */
    bool isTopActivity();

    /** @brief 延迟检查等待中的 Activity 操作 */
    void schedulePendingActivityCheck();

    /** @brief 执行等待中的 Activity 操作 */
    void processPendingActivityWork();

    /**
     * @brief 获取当前页面菜单项数量
     * @return 当前页面菜单项数量
     */
    size_t itemCount() const;

    /**
     * @brief 获取指定菜单项
     * @param index 菜单项索引
     * @return 对应菜单项，索引无效时返回 nullptr
     */
    ContextMenuEntry* itemAt(size_t index) const;

    /**
     * @brief 查询是否为独立多选页面
     * @return 当前为独立多选页面时返回 true，否则返回 false
     */
    bool isMultiSelect() const;

    /**
     * @brief 处理菜单项选择
     * @param index 被选中的菜单项索引
     */
    void handleSelection(size_t index);

    /**
     * @brief 执行普通操作
     * @param entry 被选中的普通操作菜单项
     */
    void handleAction(ContextMenuActionEntry& entry);

    /**
     * @brief 启动普通后台任务
     * @param index 被选中的菜单项索引
     * @param entry 被选中的普通后台任务菜单项
     */
    void startTask(size_t index, ContextMenuTaskEntryBase& entry);

    /**
     * @brief 处理开关选择
     * @param index 被选中的菜单项索引
     * @param entry 被选中的开关菜单项
     */
    void handleSwitch(size_t index, ContextMenuSwitchEntry& entry);

    /**
     * @brief 启动开关后台任务
     * @param index 被选中的菜单项索引
     * @param entry 被选中的开关菜单项
     */
    void startSwitch(size_t index, ContextMenuSwitchEntry& entry);

    /**
     * @brief 执行单选操作
     * @param index 被选中的菜单项索引
     * @param entry 被选中的单选菜单项
     */
    void handleRadio(size_t index, ContextMenuRadioEntry& entry);

    /**
     * @brief 进入子页面
     * @param page 需要显示的子页面共享数据
     */
    void pushPage(std::shared_ptr<ContextMenuPageData> page);

    /**
     * @brief 返回上一页或关闭菜单
     * @param animated 关闭根菜单时是否播放抽屉动画
     */
    void popPage(bool animated);

    /**
     * @brief 请求返回上一页
     * @param unlockInteraction 返回完成后是否解除列表交互锁
     */
    void requestBack(bool unlockInteraction);

    /**
     * @brief 完成普通后台任务
     * @param generation 后台任务代次
     * @param completed 后台工作函数是否正常返回
     * @param token 当前任务停止令牌
     * @param afterAction 任务完成后的页面行为
     * @param complete 任务正常完成监听
     */
    void finishTask(size_t generation, bool completed, std::stop_token token,
        ContextMenuBehaviorEntry::AfterAction afterAction, std::function<void()> complete);

    /**
     * @brief 完成开关后台任务
     * @param generation 后台任务代次
     * @param beforeState 任务开始前的真实状态
     * @param stateReader 任务结束后的真实状态读取函数
     */
    void finishSwitch(size_t generation, bool beforeState, std::function<bool()> stateReader);

    /**
     * @brief 完成普通操作或单选操作
     * @param afterAction 操作完成后的页面行为
     * @param listener 操作监听
     * @param radioAnimation 单选状态动画参数，普通操作为空
     */
    void finishBehavior(ContextMenuBehaviorEntry::AfterAction afterAction,
        std::function<void()> listener,
        std::optional<RadioAnimation> radioAnimation = std::nullopt);

    /**
     * @brief 切换多选项状态
     * @param index 多选项索引
     */
    void toggleMultiSelect(size_t index);

    /** @brief 提交多选结果 */
    void submitMultiSelect();

    /**
     * @brief 查询多选项状态
     * @param index 多选项索引
     * @return 当前选中时返回 true，否则返回 false
     */
    bool isMultiSelected(size_t index) const;

    /**
     * @brief 绑定菜单单元格
     * @param cell 需要绑定的菜单单元格
     * @param index 菜单项索引
     * @param switchAnimation 开关状态动画参数
     * @param radioAnimation 单选状态动画参数
     */
    void bindCell(ContextMenuCell& cell, size_t index,
        std::optional<SwitchAnimation> switchAnimation = std::nullopt,
        std::optional<RadioAnimation> radioAnimation = std::nullopt);

    /**
     * @brief 原地刷新全部可见单元格
     * @param switchAnimation 开关状态动画参数
     * @param radioAnimation 单选状态动画参数
     */
    void refreshVisibleCells(
        std::optional<SwitchAnimation> switchAnimation = std::nullopt,
        std::optional<RadioAnimation> radioAnimation = std::nullopt);

    /**
     * @brief 原地刷新指定可见单元格
     * @param index 菜单项索引
     */
    void refreshVisibleCell(size_t index);

    /** @brief 更新当前页面标题和宽度 */
    void configureCurrentPage();

    /**
     * @brief 精确设置滚动位置并停止旧动画
     * @param offset 目标纵向滚动位置
     */
    void setScrollOffsetExact(float offset);

    /**
     * @brief 更新提示卡片
     * @param index 当前焦点菜单项索引
     */
    void updateHint(size_t index);

    /**
     * @brief 更新页面索引
     * @param index 当前焦点菜单项索引
     */
    void updateIndex(size_t index);

    /**
     * @brief 更新页面标题图标
     * @param icon 图标资源路径，空字符串表示不显示图标
     */
    void updateTitleIcon(const std::string& icon);

    /**
     * @brief 关闭菜单
     * @param animated 是否播放抽屉关闭动画
     * @param afterClose 菜单关闭后执行的业务监听
     */
    void closeMenu(bool animated, std::function<void()> afterClose = {});

    /**
     * @brief 弹出菜单 Activity
     * @param afterClose 菜单关闭后执行的业务监听
     */
    void popMenuActivity(std::function<void()> afterClose);

    /** @brief 获取抽屉动画输入锁 */
    void acquireDrawerInputBlock();

    /** @brief 释放抽屉动画输入锁 */
    void releaseDrawerInputBlock();

    /** @brief 开始抽屉打开动画 */
    void startDrawerOpen();

    /**
     * @brief 开始抽屉关闭动画
     * @param afterClose 菜单关闭后执行的业务监听
     */
    void startDrawerClose(std::function<void()> afterClose);

    /** @brief 更新抽屉动画画面 */
    void updateDrawerViews();

    /**
     * @brief 完成抽屉动画
     * @param finished 动画是否正常完成
     */
    void finishDrawer(bool finished);

    std::shared_ptr<ContextMenuPageData> m_rootPage;             // 普通根页面
    std::shared_ptr<ContextMultiSelectPageData> m_multiPage;     // 独立多选页面
    std::vector<PageStackEntry> m_pageStack;                     // 普通页面栈
    std::vector<bool> m_multiSelected;                           // 多选页面选中状态
    size_t m_multiSelectedCount = 0;                             // 多选页面已选数量

    std::optional<FocusFrame> m_previousFocusFrame;              // 菜单打开前的焦点位置
    std::shared_ptr<Session> m_session = std::make_shared<Session>(); // 菜单业务生命周期
    std::shared_ptr<Session> m_viewSession = std::make_shared<Session>(); // 菜单视图生命周期

    std::stop_source m_stopSource;                               // 当前任务停止源
    WaitableTask m_runningTask;                                  // 当前后台任务
    RunningKind m_runningKind = RunningKind::None;               // 当前后台任务类型
    size_t m_activeIndex = NO_INDEX;                              // 当前执行项索引
    size_t m_runGeneration = 0;                                  // 后台任务代次
    bool m_activeTaskCancelable = false;                          // 普通任务是否允许取消

    bool m_initialized = false;                                  // 根页面是否已加载
    bool m_closing = false;                                      // 菜单是否正在关闭
    bool m_activityPaused = false;                               // 菜单 Activity 是否暂停
    bool m_activityCheckScheduled = false;                       // 是否已排队检查 Activity
    bool m_popActivityPending = false;                            // 是否等待关闭菜单 Activity
    bool m_backPending = false;                                  // 是否等待返回上一级
    std::function<void()> m_pendingAfterClose;                    // 延迟关闭后的业务监听
    bool m_drawerInputBlocked = false;                            // 抽屉动画是否持有输入锁
    brls::Animatable m_drawerAnimation{0.0f};                     // 抽屉动画进度
    float m_drawerDistance = 0.0f;                               // 抽屉移动距离
    std::function<void()> m_drawerDone;                           // 抽屉动画完成监听

    BRLS_BIND(brls::Label, m_title, "contextMenu/title");
    BRLS_BIND(brls::Image, m_titleIcon, "contextMenu/titleIcon");
    BRLS_BIND(brls::Label, m_version, "contextMenu/version");
    BRLS_BIND(RecyclingGrid, m_grid, "contextMenu/grid");
    BRLS_BIND(brls::Label, m_index, "contextMenu/index");
    BRLS_BIND(brls::Label, m_hintTitle, "contextMenu/hintTitle");
    BRLS_BIND(brls::Label, m_hint, "contextMenu/hint");
    BRLS_BIND(brls::Box, m_hintCard, "contextMenu/hintCard");
    BRLS_BIND(brls::Box, m_left, "contextMenu/left");
    BRLS_BIND(brls::Box, m_panel, "contextMenu/panel");
};
