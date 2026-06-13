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
#include <string>
#include <vector>
#include <functional>

// ── DialogButton ──
// brls::Button 子类，焦点切换时即时隐藏高亮和清除点击脉冲

class DialogButton : public brls::Button {
public:
    DialogButton();
    void onFocusGained() override;
    void onFocusLost() override;
    static brls::View* create();
};

// ── DialogButtonBox ──
// 对话框按钮容器，导航成功时播放 Focus 音效

class DialogButtonBox : public brls::Box {
public:
    brls::View* getNextFocus(brls::FocusDirection direction, brls::View* currentView) override;
    static brls::View* create();
};

class CustomDialog : public brls::Box
{
  public:
    // 按钮配置（值类型，不需要手动 delete）
    struct ButtonConfig
    {
        std::string label;
        std::function<void()> cb;
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

    std::vector<ButtonConfig> buttons;

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
    static void show(std::string text,
        std::vector<ButtonConfig> buttons = {},
        std::function<void()> bAction = nullptr);

    brls::View* getNextFocus(brls::FocusDirection direction, brls::View* currentView) override;

    brls::AppletFrame* getAppletFrame() override;

    bool isTranslucent() override { return true; }

    /** @brief 获取内容容器（供替换式机制访问） */
    brls::Box* getContainer();
};
