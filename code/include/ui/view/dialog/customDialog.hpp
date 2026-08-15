/**
 * CustomDialog - 自定义对话框
 *
 * 基于框架 brls::Dialog 的结构，照抄其写法风格，
 * 在此基础上做了三点改动：
 *   1. 按钮点击不会自动关闭对话框（由回调自己决定）
 *   2. B 键支持自定义回调（onB），默认为 close()
 *   3. open() 支持替换式：已有对话框时替换内容，遮罩不闪烁
 */

#pragma once

#include <borealis.hpp>
#include <functional>
#include <string>
#include <vector>

/** @brief 对话框按钮，负责在焦点切换时更新高亮和点击脉冲 */
class DialogButton : public brls::Button {
public:
    /** @brief 创建对话框按钮 */
    DialogButton();

    /** @brief 获得焦点时恢复高亮和点击动画 */
    void onFocusGained() override;

    /** @brief 失去焦点时隐藏高亮并清除点击脉冲 */
    void onFocusLost() override;

    /** @brief XML View 工厂函数 */
    static brls::View* create();
};

/** @brief 对话框按钮容器，导航成功时播放焦点音效 */
class DialogButtonBox : public brls::Box {
public:
    /** @brief 获取下一个焦点，并在导航成功时播放焦点音效 */
    brls::View* getNextFocus(brls::FocusDirection direction, brls::View* currentView) override;

    /** @brief XML View 工厂函数 */
    static brls::View* create();
};

/** @brief 支持自定义按钮、B 键行为和内容替换的对话框 */
class CustomDialog : public brls::Box {
public:
    // 按钮配置（值类型，不需要手动 delete）
    struct ButtonConfig {
        std::string label;          // 按钮文本
        std::function<void()> cb;   // 点击回调
    };

private:
    // ── XML 子 View 绑定（和框架 Dialog 一样） ──

    BRLS_BIND(brls::Box, container, "custom_dialog/container");
    BRLS_BIND(brls::AppletFrame, appletFrame, "custom_dialog/applet");

    BRLS_BIND(DialogButton, button1, "custom_dialog/button1");
    BRLS_BIND(DialogButton, button2, "custom_dialog/button2");
    BRLS_BIND(DialogButton, button3, "custom_dialog/button3");

    BRLS_BIND(brls::Rectangle, button2separator, "custom_dialog/button2/separator");
    BRLS_BIND(brls::Rectangle, button3separator, "custom_dialog/button3/separator");

    // ── 内部状态 ──

    // B 键自定义回调，为空时默认执行 close()
    std::function<void()> bAction;

    std::vector<ButtonConfig> buttons; // 当前按钮配置，最多三项

    // 文本 Label 指针（仅文本构造函数设置，Box 构造函数为 nullptr）
    brls::Label* textLabel = nullptr;

    // 替换式机制：追踪当前显示的对话框实例
    static CustomDialog* s_current;

    /** @brief 根据 buttons 列表刷新按钮显示 */
    void rebuildButtons();

    /** @brief 注册 B 键 action 到 appletFrame */
    void registerBAction();

public:
    /**
     * @brief 简单文本对话框
     * @param text 显示文本
     */
    CustomDialog(std::string text);

    /**
     * @brief 自定义布局对话框
     * @param contentView 内容视图
     */
    CustomDialog(brls::Box* contentView);

    /**
     * @brief 添加按钮（最多 3 个，仅 open() 前调用）
     * @param label 按钮文本
     * @param cb 点击回调
     */
    void addButton(std::string label, std::function<void()> cb = [] {});

    /** @brief 隐藏所有按钮并禁用 B 键（用于不可取消状态） */
    static void hideButtons();

    /**
     * @brief 设置 B 键自定义回调（默认执行 close()）
     * @param action 回调函数
     */
    void onB(std::function<void()> action);

    /** @brief 显示对话框（替换式：已有对话框时替换内容，遮罩不闪烁） */
    virtual void open();

    /**
     * @brief 关闭当前对话框
     * @param cb 关闭后回调
     */
    static void close(std::function<void()> cb = [] {});

    /**
     * @brief 更新当前对话框的文本
     * @param text 新文本
     */
    static void setText(std::string text);

    /**
     * @brief 便捷方法：创建 + 添加按钮 + 打开
     * @param text 显示文本
     * @param buttons 按钮列表
     * @param bAction B 键回调
     */
    static void show(std::string text, std::vector<ButtonConfig> buttons = {}, std::function<void()> bAction = nullptr);

    /** @brief 获取下一个焦点，并在到达边界时播放提示音效 */
    brls::View* getNextFocus(brls::FocusDirection direction, brls::View* currentView) override;

    /** @brief 获取对话框承载的 AppletFrame */
    brls::AppletFrame* getAppletFrame() override;

    /** @brief 对话框使用半透明背景 */
    bool isTranslucent() override { return true; }

    /** @brief 获取内容容器（供替换式机制访问） */
    brls::Box* getContainer();
};
