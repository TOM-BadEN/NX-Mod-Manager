/**
 * CustomDialog - 自定义对话框实现
 *
 * 代码结构照抄框架 brls::Dialog，改动点用注释标出。
 */
 
#include "view/customDialog.hpp"
#include <borealis/core/i18n.hpp>
#include "core/audio.hpp"
#include "utils/pageNav.hpp"
 
using namespace brls::literals;

// ── DialogButton ──────────────────────────────────────────────

DialogButton::DialogButton() = default;

void DialogButton::onFocusGained() {
    setHideHighlight(false);
    setHideClickAnimation(false);
    Button::onFocusGained();
}

void DialogButton::onFocusLost() {
    setHideHighlight(true);
    playClickAnimation(false, false, true);
    resetClickAnimation();
    setHideClickAnimation(true);
    Button::onFocusLost();
}

brls::View* DialogButton::create() { return new DialogButton(); }

// ── DialogButtonBox ────────────────────────────────────────────────────

brls::View* DialogButtonBox::getNextFocus(brls::FocusDirection direction, brls::View* currentView) {
    auto* next = Box::getNextFocus(direction, currentView);
    if (next) Audio::instance()->play(SoundEffect::Focus);
    return next;
}

brls::View* DialogButtonBox::create() { return new DialogButtonBox(); }

// ── 替换式机制的静态指针，追踪当前显示的对话框 ──

CustomDialog* CustomDialog::s_current = nullptr;

// XML 布局在 resources/xml/view/customDialog.xml
// 和框架 Dialog 完全一样，只换了 id 前缀

// ── 构造函数（自定义布局版）──
// 和框架 Dialog::Dialog(Box* contentView) 一样

CustomDialog::CustomDialog(brls::Box* contentView)
{
    this->inflateFromXMLRes("xml/view/customDialog.xml");
    container->addView(contentView);
    this->registerBAction();
}

// ── 构造函数（文本版）──
// 和框架 Dialog::Dialog(std::string text) 一样

CustomDialog::CustomDialog(std::string text)
{
    brls::Style style = brls::Application::getStyle();

    // 创建文本 Label
    brls::Label* label = new brls::Label();
    label->setText(text);
    label->setFontSize(style["brls/dialog/fontSize"]);
    label->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    label->setSingleLine(false);

    // 包在 Box 里，加 padding
    brls::Box* box = new brls::Box();
    box->addView(label);
    box->setAlignItems(brls::AlignItems::CENTER);
    box->setJustifyContent(brls::JustifyContent::CENTER);
    box->setPadding(
        style["brls/dialog/paddingTopBottom"],
        style["brls/dialog/paddingLeftRight"],
        style["brls/dialog/paddingTopBottom"],
        style["brls/dialog/paddingLeftRight"]);

    this->textLabel = label;

    this->inflateFromXMLRes("xml/view/customDialog.xml");
    container->addView(box);
    this->registerBAction();
}

// ── addButton ──
// 和框架一样，最多 3 个，仅 open 前调用

void CustomDialog::addButton(std::string label, std::function<void()> cb)
{
    if (this->buttons.size() >= 3)
        return;

    this->buttons.push_back({label, std::move(cb)});
    this->rebuildButtons();
}

void CustomDialog::hideButtons()
{
    if (!s_current) return;
    s_current->buttons.clear();
    s_current->button1->getParent()->setVisibility(brls::Visibility::GONE);
    s_current->setFocusable(true);
    s_current->setHideHighlight(true);
    brls::Application::giveFocus(s_current);
    s_current->onB([] {});
}

// ── rebuildButtons ──
// 和框架 rebuildButtons 结构一样，区别：
//   1. 先重置所有按钮为隐藏（支持替换式刷新）
//   2. 点击回调直接执行 cb()，不调用 dismiss

void CustomDialog::rebuildButtons()
{
    using brls::Visibility;

    // 先重置（框架不需要这步，因为按钮只增不减；我们支持替换，需要重置）
    button1->getParent()->setVisibility(Visibility::GONE);
    button1->setVisibility(Visibility::GONE);
    button2->setVisibility(Visibility::GONE);
    button3->setVisibility(Visibility::GONE);
    button2separator->setVisibility(Visibility::GONE);
    button3separator->setVisibility(Visibility::GONE);

    // 第 1 个按钮
    if (this->buttons.size() > 0)
    {
        setLastFocusedView(button1);
        button1->getParent()->setVisibility(Visibility::VISIBLE);

        button1->setVisibility(Visibility::VISIBLE);
        button1->setText(buttons[0].label);
        // 【改动点】直接执行回调，不 dismiss
        auto cb0 = buttons[0].cb;
        button1->registerClickAction([cb0](brls::View*) {
            Audio::instance()->play(SoundEffect::Enter);
            cb0();
            return true;
        });
    }

    // 第 2 个按钮
    if (this->buttons.size() > 1)
    {
        button2separator->setVisibility(Visibility::VISIBLE);
        button2->setVisibility(Visibility::VISIBLE);
        button2->setText(buttons[1].label);
        auto cb1 = buttons[1].cb;
        button2->registerClickAction([cb1](brls::View*) {
            Audio::instance()->play(SoundEffect::Enter);
            cb1();
            return true;
        });
    }

    // 第 3 个按钮
    if (this->buttons.size() > 2)
    {
        button3separator->setVisibility(Visibility::VISIBLE);
        button3->setVisibility(Visibility::VISIBLE);
        button3->setText(buttons[2].label);
        auto cb2 = buttons[2].cb;
        button3->registerClickAction([cb2](brls::View*) {
            Audio::instance()->play(SoundEffect::Enter);
            cb2();
            return true;
        });
    }
}

// ── onB ──
// 设置自定义 B 键回调

void CustomDialog::onB(std::function<void()> action)
{
    this->bAction = std::move(action);
    this->registerBAction();
}

// ── registerBAction ──
// 注册 B 键到 appletFrame
// 【改动点】框架只能 dismiss 或禁用；这里支持自定义回调

void CustomDialog::registerBAction()
{
    // 如果用户没设置 onB，默认关闭对话框
    auto action = this->bAction ? this->bAction : [] { CustomDialog::close(); };

    appletFrame->registerAction(
        brls::getStr("views/customDialog/back"), brls::BUTTON_B,
        [action](brls::View*) {
            Audio::instance()->play(SoundEffect::Enter);
            action();
            return true;
        });
}

// ── open ──
// 【改动点】替换式机制：已有对话框时替换内容，遮罩不闪烁

void CustomDialog::open()
{
    // 防止对同一个对话框重复调用 open()
    if (this == s_current)
        return;

    if (s_current)
    {
        // 已有对话框在显示 → 替换内容，不推新 Activity

        // 从自己的 container 中取出内容（不释放）
        auto& children = container->getChildren();
        brls::View* content = children.empty() ? nullptr : children[0];
        if (content)
            container->removeView(content, false);

        // 清空旧对话框的内容，放入新内容
        s_current->container->clearViews();
        if (content)
            s_current->container->addView(content);

        // 转移按钮配置并刷新
        s_current->buttons = std::move(this->buttons);
        s_current->rebuildButtons();

        // 清除残留点击脉冲（clickAlpha 归零），不禁用动画能力
        s_current->button1->playClickAnimation(false, false, true);
        s_current->button1->resetClickAnimation();
        s_current->button2->playClickAnimation(false, false, true);
        s_current->button2->resetClickAnimation();
        s_current->button3->playClickAnimation(false, false, true);
        s_current->button3->resetClickAnimation();

        // 重置焦点
        if (!s_current->buttons.empty())
        {
            brls::Application::giveFocus(s_current->button1);
        }
        else
        {
            // 没有按钮时，焦点给对话框自身，隐藏高亮框
            s_current->setFocusable(true);
            s_current->setHideHighlight(true);
            brls::Application::giveFocus(s_current);
        }

        // 转移 textLabel
        s_current->textLabel = this->textLabel;

        // 转移 B 键回调并重新注册
        s_current->bAction = std::move(this->bAction);
        s_current->registerBAction();

        // 自己只是配置载体，转移完即销毁
        delete this;
    }
    else
    {
        // 没有对话框在显示 → 正常推 Activity（和框架一样）
        s_current = this;
        if (buttons.empty()) {
            this->setFocusable(true);
            this->setHideHighlight(true);
        }
        pageNav::push(new brls::Activity(this));
    }
}

// ── close ──
// 静态方法，关闭当前显示的对话框

void CustomDialog::close(std::function<void()> cb)
{
    if (!s_current)
        return;
    s_current = nullptr;
    brls::Application::popActivity(brls::TransitionAnimation::NONE, cb);
}

// ── setText ──
// 静态方法，更新当前对话框的文本

void CustomDialog::setText(std::string text)
{
    if (s_current && s_current->textLabel)
        s_current->textLabel->setText(text);
}

// ── show ──
// 便捷方法：创建 + 添加按钮 + 打开，一步到位

void CustomDialog::show(std::string text, std::vector<ButtonConfig> buttons,
    std::function<void()> bAction)
{
    auto* dlg = new CustomDialog(std::move(text));
    for (auto& btn : buttons)
        dlg->addButton(std::move(btn.label), std::move(btn.cb));
    if (bAction)
        dlg->onB(std::move(bAction));
    dlg->open();
}

// ── getAppletFrame ──
// 和框架一样

brls::View* CustomDialog::getNextFocus(brls::FocusDirection direction, brls::View* currentView) {
    auto* next = Box::getNextFocus(direction, currentView);
    if (!next) Audio::instance()->play(SoundEffect::FocusLimit);
    return next;
}

brls::AppletFrame* CustomDialog::getAppletFrame()
{
    return appletFrame;
}

// ── getContainer ──
// 供替换式机制访问内容区

brls::Box* CustomDialog::getContainer()
{
    return container;
}
