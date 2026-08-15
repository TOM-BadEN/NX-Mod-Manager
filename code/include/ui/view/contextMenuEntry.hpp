/**
 * ContextMenuEntry - 上下文菜单项
 *
 * 每种菜单项只保存自己的状态和行为，公共基类仅提供显示属性。
 */

#pragma once

#include <functional>
#include <memory>
#include <stop_token>
#include <string>
#include <type_traits>
#include <utility>

class ContextMenu;
class ContextMenuPage;
class ContextMultiSelectPage;
struct ContextMenuPageData;

/** @brief 上下文菜单项公共显示属性 */
class ContextMenuEntry {
public:
    virtual ~ContextMenuEntry() = default;

    ContextMenuEntry(const ContextMenuEntry&) = delete;
    ContextMenuEntry& operator=(const ContextMenuEntry&) = delete;
    ContextMenuEntry(ContextMenuEntry&&) = delete;
    ContextMenuEntry& operator=(ContextMenuEntry&&) = delete;

    /**
     * @brief 设置静态徽章
     * @param badge 徽章文字，空字符串表示不显示徽章
     */
    void setBadge(const std::string& badge);

    /**
     * @brief 设置动态徽章读取函数
     * @param badge 返回当前徽章文字的读取函数
     */
    void setBadge(std::function<std::string()> badge);

    /**
     * @brief 设置图标资源路径
     * @param path 图标资源路径，空字符串表示不显示图标
     */
    void setIcon(const std::string& path);

    /**
     * @brief 设置动态禁用条件
     * @param disabled 返回当前禁用状态的读取函数，返回 true 表示禁用
     */
    void setDisabled(std::function<bool()> disabled);

    /**
     * @brief 设置动态徽章高亮条件
     * @param highlighted 返回当前徽章高亮状态的读取函数，返回 true 表示高亮
     */
    void setBadgeHighlight(std::function<bool()> highlighted);

protected:
    enum class Kind {
        Action,
        Task,
        Switch,
        Radio,
        Submenu,
        MultiSelectOption,
    };

    /**
     * @brief 创建上下文菜单项
     * @param title 菜单项标题
     * @param hint 菜单项提示文字
     */
    ContextMenuEntry(const std::string& title, const std::string& hint);

private:
    friend class ContextMenu;

    /**
     * @brief 获取菜单项类型
     * @return 当前具体菜单项类型
     */
    virtual Kind kind() const = 0;

    /**
     * @brief 获取菜单项标题
     * @return 菜单项标题
     */
    const std::string& title() const;

    /**
     * @brief 获取菜单项提示文字
     * @return 菜单项提示文字
     */
    const std::string& hint() const;

    /**
     * @brief 获取图标资源路径
     * @return 图标资源路径
     */
    const std::string& icon() const;

    /**
     * @brief 获取当前徽章文字
     * @return 动态徽章读取结果，未设置动态读取函数时返回静态徽章
     */
    std::string badge() const;

    /**
     * @brief 获取当前禁用状态
     * @return 禁用时返回 true，否则返回 false
     */
    bool disabled() const;

    /**
     * @brief 获取当前徽章高亮状态
     * @return 高亮时返回 true，否则返回 false
     */
    bool badgeHighlighted() const;

    std::string m_title;                                     // 标题
    std::string m_hint;                                      // 提示文字
    std::string m_badge;                                     // 静态徽章
    std::string m_icon;                                      // 图标资源路径
    std::function<std::string()> m_badgeProvider;             // 动态徽章读取函数
    std::function<bool()> m_disabledProvider;                 // 动态禁用条件
    std::function<bool()> m_badgeHighlightProvider;           // 动态徽章高亮条件
};

/** @brief 带执行后页面行为的菜单项 */
class ContextMenuBehaviorEntry : public ContextMenuEntry {
public:
    /** @brief 执行后返回上一级，根页面则关闭菜单 */
    void setBack();

    /** @brief 执行后停留在当前页面 */
    void setStayOpen();

protected:
    /**
     * @brief 创建带页面行为的菜单项
     * @param title 菜单项标题
     * @param hint 菜单项提示文字
     */
    ContextMenuBehaviorEntry(const std::string& title, const std::string& hint);

private:
    friend class ContextMenu;

    enum class AfterAction {
        Close,
        Back,
        Stay,
    };

    /**
     * @brief 获取执行后的页面行为
     * @return 当前页面行为
     */
    AfterAction afterAction() const;

    AfterAction m_afterAction = AfterAction::Close;           // 执行后的页面行为
};

/** @brief 普通操作菜单项 */
class ContextMenuActionEntry final : public ContextMenuBehaviorEntry {
public:
    /**
     * @brief 设置选中监听
     * @param selected 菜单项选中后执行的监听
     */
    void onSelected(std::function<void()> selected);

private:
    friend class ContextMenu;
    friend class ContextMenuPage;

    /**
     * @brief 创建普通操作菜单项
     * @param title 菜单项标题
     * @param hint 菜单项提示文字
     */
    ContextMenuActionEntry(const std::string& title, const std::string& hint);

    /**
     * @brief 获取菜单项类型
     * @return 普通操作类型
     */
    Kind kind() const override;

    /**
     * @brief 获取选中监听
     * @return 当前选中监听
     */
    std::function<void()> selectedListener() const;

    std::function<void()> m_selected;                         // 选中监听
};

/** @brief 普通后台任务菜单项基类 */
class ContextMenuTaskEntryBase : public ContextMenuBehaviorEntry {
public:
    /**
     * @brief 设置是否允许 B 键请求取消
     * @param cancelable 是否允许请求取消
     */
    void setCancelable(bool cancelable);

protected:
    using Finish = std::function<void(bool, std::function<void()>)>;
    using Worker = std::function<void(std::stop_token, Finish)>;

    /**
     * @brief 创建普通后台任务菜单项基类
     * @param title 菜单项标题
     * @param hint 菜单项提示文字
     */
    ContextMenuTaskEntryBase(const std::string& title, const std::string& hint);

private:
    friend class ContextMenu;

    /**
     * @brief 获取菜单项类型
     * @return 普通后台任务类型
     */
    Kind kind() const override;

    /**
     * @brief 获取任务取消设置
     * @return 允许 B 键请求取消时返回 true，否则返回 false
     */
    bool cancelable() const;

    /**
     * @brief 查询是否已设置后台任务
     * @return 已设置后台任务时返回 true，否则返回 false
     */
    virtual bool hasTask() const = 0;

    /**
     * @brief 创建后台任务执行函数
     * @return 包含任务和完成监听的执行函数
     */
    virtual Worker makeWorker() const = 0;

    bool m_cancelable = false;                                // 是否允许 B 键请求取消
};

/** @brief 带类型结果的普通后台任务菜单项 */
template <typename Result>
class ContextMenuTaskEntry final : public ContextMenuTaskEntryBase {
    static_assert(!std::is_void_v<Result>, "void tasks use ContextMenuTaskEntry<void>");
    static_assert(!std::is_reference_v<Result>, "task results must be value types");
    static_assert(std::is_move_constructible_v<Result>, "task results must be move-constructible value types");

public:
    /**
     * @brief 设置后台任务
     * @param task 接收停止令牌并返回任务结果的后台函数
     */
    void setTask(std::function<Result(std::stop_token)> task) {
        m_task = std::move(task);
    }

    /**
     * @brief 设置任务正常完成监听
     * @param complete 接收任务结果的界面线程监听
     */
    void onComplete(std::function<void(Result)> complete) {
        m_complete = std::move(complete);
    }

private:
    friend class ContextMenuPage;

    /**
     * @brief 创建带结果的普通后台任务菜单项
     * @param title 菜单项标题
     * @param hint 菜单项提示文字
     */
    ContextMenuTaskEntry(const std::string& title, const std::string& hint) : ContextMenuTaskEntryBase(title, hint) {}

    /**
     * @brief 查询是否已设置后台任务
     * @return 已设置后台任务时返回 true，否则返回 false
     */
    bool hasTask() const override {
        return static_cast<bool>(m_task);
    }

    /**
     * @brief 创建后台任务执行函数
     * @return 包含任务和完成监听的执行函数
     */
    Worker makeWorker() const override {
        auto task = m_task;
        auto complete = m_complete;

        return [task = std::move(task), complete = std::move(complete)](std::stop_token token, Finish finish) mutable {
            if (!task || token.stop_requested()) {
                finish(false, {});
                return;
            }

            Result result = task(token);
            if (token.stop_requested()) {
                finish(false, {});
                return;
            }

            if (!complete) {
                finish(true, {});
                return;
            }

            auto resultHolder = std::make_shared<Result>(std::move(result));
            finish(true, [complete = std::move(complete), resultHolder]() mutable {
                complete(std::move(*resultHolder));
            });
        };
    }

    std::function<Result(std::stop_token)> m_task;             // 后台任务
    std::function<void(Result)> m_complete;                    // 正常完成监听
};

/** @brief 无结果的普通后台任务菜单项 */
template <>
class ContextMenuTaskEntry<void> final : public ContextMenuTaskEntryBase {
public:
    /**
     * @brief 设置后台任务
     * @param task 接收停止令牌的无结果后台函数
     */
    void setTask(std::function<void(std::stop_token)> task) {
        m_task = std::move(task);
    }

    /**
     * @brief 设置任务正常完成监听
     * @param complete 任务正常完成后在界面线程执行的监听
     */
    void onComplete(std::function<void()> complete) {
        m_complete = std::move(complete);
    }

private:
    friend class ContextMenuPage;

    /**
     * @brief 创建无结果的普通后台任务菜单项
     * @param title 菜单项标题
     * @param hint 菜单项提示文字
     */
    ContextMenuTaskEntry(const std::string& title, const std::string& hint) : ContextMenuTaskEntryBase(title, hint) {}

    /**
     * @brief 查询是否已设置后台任务
     * @return 已设置后台任务时返回 true，否则返回 false
     */
    bool hasTask() const override {
        return static_cast<bool>(m_task);
    }

    /**
     * @brief 创建后台任务执行函数
     * @return 包含任务和完成监听的执行函数
     */
    Worker makeWorker() const override {
        auto task = m_task;
        auto complete = m_complete;

        return [task = std::move(task), complete = std::move(complete)](std::stop_token token, Finish finish) mutable {
            if (!task || token.stop_requested()) {
                finish(false, {});
                return;
            }

            task(token);
            if (token.stop_requested()) {
                finish(false, {});
                return;
            }

            finish(true, std::move(complete));
        };
    }

    std::function<void(std::stop_token)> m_task;               // 后台任务
    std::function<void()> m_complete;                          // 正常完成监听
};

/** @brief 开关菜单项 */
class ContextMenuSwitchEntry final : public ContextMenuEntry {
public:
    /**
     * @brief 设置真实状态读取函数
     * @param state 返回当前真实开关状态的读取函数，返回 true 表示开启
     */
    void setState(std::function<bool()> state);

    /**
     * @brief 设置开关后台任务
     * @param task 开关后台函数，参数 true 表示开启
     */
    void setTask(std::function<void(bool)> task);

    /**
     * @brief 设置开启前的确认说明
     * @param message 确认弹窗文字，空字符串表示开启时不需要确认
     */
    void setEnableConfirmation(const std::string& message);

    /**
     * @brief 设置关闭前的确认说明
     * @param message 确认弹窗文字，空字符串表示关闭时不需要确认
     */
    void setDisableConfirmation(const std::string& message);

private:
    friend class ContextMenu;
    friend class ContextMenuPage;

    /**
     * @brief 创建开关菜单项
     * @param title 菜单项标题
     * @param hint 菜单项提示文字
     */
    ContextMenuSwitchEntry(const std::string& title, const std::string& hint);

    /**
     * @brief 获取菜单项类型
     * @return 开关类型
     */
    Kind kind() const override;

    /**
     * @brief 获取真实状态读取函数
     * @return 当前真实状态读取函数
     */
    std::function<bool()> stateReader() const;

    /**
     * @brief 获取开关后台任务
     * @return 当前开关后台任务
     */
    std::function<void(bool)> task() const;

    /**
     * @brief 获取切换前的确认说明
     * @param requestedState 准备切换到的状态，true 表示开启
     * @return 对应状态的确认弹窗文字，空字符串表示不需要确认
     */
    const std::string& confirmation(bool requestedState) const;

    std::function<bool()> m_state;                             // 真实状态读取函数
    std::function<void(bool)> m_task;                          // 开关后台任务
    std::string m_enableConfirmation;                          // 开启前的确认说明
    std::string m_disableConfirmation;                         // 关闭前的确认说明
};

/** @brief 单选菜单项 */
class ContextMenuRadioEntry final : public ContextMenuBehaviorEntry {
public:
    /**
     * @brief 设置真实选中状态读取函数
     * @param selected 返回当前真实选中状态的读取函数，返回 true 表示选中
     */
    void setSelected(std::function<bool()> selected);

    /**
     * @brief 设置选中监听
     * @param selected 菜单项选中后执行的监听
     */
    void onSelected(std::function<void()> selected);

private:
    friend class ContextMenu;
    friend class ContextMenuPage;

    /**
     * @brief 创建单选菜单项
     * @param title 菜单项标题
     * @param hint 菜单项提示文字
     */
    ContextMenuRadioEntry(const std::string& title, const std::string& hint);

    /**
     * @brief 获取菜单项类型
     * @return 单选类型
     */
    Kind kind() const override;

    /**
     * @brief 获取真实选中状态读取函数
     * @return 当前真实选中状态读取函数
     */
    std::function<bool()> selectedReader() const;

    /**
     * @brief 获取选中监听
     * @return 当前选中监听
     */
    std::function<void()> selectedListener() const;

    std::function<bool()> m_selectedReader;                    // 真实选中状态读取函数
    std::function<void()> m_selected;                          // 选中监听
};

/** @brief 子菜单入口 */
class ContextMenuSubmenuEntry final : public ContextMenuEntry {
public:
    /**
     * @brief 设置子菜单页面，内部共享持有页面数据
     * @param page 进入此菜单项后显示的页面
     */
    void setPage(const ContextMenuPage& page);

private:
    friend class ContextMenu;
    friend class ContextMenuPage;

    /**
     * @brief 创建子菜单入口
     * @param title 菜单项标题
     * @param hint 菜单项提示文字
     */
    ContextMenuSubmenuEntry(const std::string& title, const std::string& hint);

    /**
     * @brief 获取菜单项类型
     * @return 子菜单类型
     */
    Kind kind() const override;

    /**
     * @brief 获取子菜单页面
     * @return 子菜单页面共享数据
     */
    std::shared_ptr<ContextMenuPageData> page() const;

    /**
     * @brief 检查目标页面是否会形成页面所有权环
     * @param page 需要检查的目标页面
     * @return 会形成环时返回 true，否则返回 false
     */
    bool wouldCreateCycle(const std::shared_ptr<ContextMenuPageData>& page) const;

    std::weak_ptr<ContextMenuPageData> m_ownerPage;             // 所属页面
    std::shared_ptr<ContextMenuPageData> m_page;                // 子菜单页面
};

/** @brief 多选页面选项 */
class ContextMultiSelectOption final : public ContextMenuEntry {
public:
    /**
     * @brief 设置页面打开时的初始选中状态
     * @param selected 是否初始选中
     */
    void setSelected(bool selected);

private:
    friend class ContextMenu;
    friend class ContextMultiSelectPage;

    /**
     * @brief 创建多选页面选项
     * @param title 选项标题
     * @param hint 选项提示文字
     */
    ContextMultiSelectOption(const std::string& title, const std::string& hint);

    /**
     * @brief 获取菜单项类型
     * @return 多选页面选项类型
     */
    Kind kind() const override;

    /**
     * @brief 获取初始选中状态
     * @return 初始选中时返回 true，否则返回 false
     */
    bool selected() const;

    bool m_selected = false;                                  // 初始选中状态
};
