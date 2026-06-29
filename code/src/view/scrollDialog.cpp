/**
 * ScrollDialog - 可滚动长文本对话框实现
 *
 * 基于 CustomDialog(Box*) 容器，不使用其自带按钮。
 * 滚动区域和按钮均定义在 scrollDialog.xml 中。
 */

#include "view/scrollDialog.hpp"
#include "core/audio.hpp"

namespace {

brls::ScrollingFrame* s_scroll = nullptr;
int s_atBoundary = 0;  // 记录到达边界的方向（0=未到边界, 1=上, 2=下），防止长按同方向重复抖动

// ── 滚动辅助 ──────────────────────────────────────────

bool isAtTop() {
    return s_scroll->getContentOffsetY() <= 0;
}

bool isAtBottom() {
    auto& children = s_scroll->getChildren();
    float contentH = children.empty() ? 0 : children[0]->getHeight();
    float bottomLimit = contentH - s_scroll->getHeight();
    return s_scroll->getContentOffsetY() >= bottomLimit - 0.01f;
}

bool tryScroll(brls::View* view, float delta, brls::FocusDirection dir, bool (*atLimit)()) {
    if (!s_scroll) return true;
    int boundaryId = 2;
    if (dir == brls::FocusDirection::UP) boundaryId = 1;
    if (atLimit()) {
        if (s_atBoundary != boundaryId) {
            Audio::instance()->play(SoundEffect::FocusLimit);
            view->shakeHighlight(dir);
            s_atBoundary = boundaryId;
        }
        return true;
    }
    s_atBoundary = 0;
    s_scroll->setContentOffsetY(s_scroll->getContentOffsetY() + delta, true);
    return true;
}

// ── 按钮配置 ──────────────────────────────────────────

void setupClickAction(DialogButton* btn, std::function<void()> cb) {
    if (!btn) return;
    btn->registerClickAction([cb = std::move(cb)](brls::View*) {
        Audio::instance()->play(SoundEffect::Enter);
        cb();
        return true;
    });
}

void setupScrollActions(DialogButton* btn) {
    if (!btn) return;
    btn->registerAction("", brls::BUTTON_NAV_UP, [](brls::View* v) {
        return tryScroll(v, -60.0f, brls::FocusDirection::UP, isAtTop);
    }, true, true);
    btn->registerAction("", brls::BUTTON_NAV_DOWN, [](brls::View* v) {
        return tryScroll(v, 60.0f, brls::FocusDirection::DOWN, isAtBottom);
    }, true, true);
}

void setupNavigation(DialogButton* btn1, DialogButton* btn2) {
    if (!btn1 || !btn2) return;

    // 左右切换焦点
    btn1->registerAction("", brls::BUTTON_NAV_RIGHT, [btn2](...) {
        s_atBoundary = 0;
        Audio::instance()->play(SoundEffect::Focus);
        brls::Application::giveFocus(btn2);
        return true;
    }, true);
    btn2->registerAction("", brls::BUTTON_NAV_LEFT, [btn1](...) {
        s_atBoundary = 0;
        Audio::instance()->play(SoundEffect::Focus);
        brls::Application::giveFocus(btn1);
        return true;
    }, true);

    // 边界抖动
    btn1->registerAction("", brls::BUTTON_NAV_LEFT, [btn1](...) {
        s_atBoundary = 0;
        Audio::instance()->play(SoundEffect::FocusLimit);
        btn1->shakeHighlight(brls::FocusDirection::LEFT);
        return true;
    }, true);
    btn2->registerAction("", brls::BUTTON_NAV_RIGHT, [btn2](...) {
        s_atBoundary = 0;
        Audio::instance()->play(SoundEffect::FocusLimit);
        btn2->shakeHighlight(brls::FocusDirection::RIGHT);
        return true;
    }, true);
}

} // anonymous namespace

// ── 公开 API ──────────────────────────────────────────

namespace ScrollDialog {

void show(const std::string& text,
    const std::string& btn1Label, std::function<void()> btn1Cb,
    const std::string& btn2Label, std::function<void()> btn2Cb,
    std::function<void()> bAction)
{
    auto* content = dynamic_cast<brls::Box*>(
        brls::View::createFromXMLResource("view/scrollDialog.xml"));
    if (!content) return;

    // 绑定视图
    s_scroll = dynamic_cast<brls::ScrollingFrame*>(content->getView("scroll_dialog/scroll"));
    auto* label = dynamic_cast<brls::Label*>(content->getView("scroll_dialog/text"));
    auto* btn1  = dynamic_cast<DialogButton*>(content->getView("scroll_dialog/btn1"));
    auto* btn2  = dynamic_cast<DialogButton*>(content->getView("scroll_dialog/btn2"));

    // 设置内容
    if (label) label->setText(text);
    if (btn1) btn1->setText(btn1Label);
    if (btn2) btn2->setText(btn2Label);

    // 注册按钮行为
    setupClickAction(btn1, std::move(btn1Cb));
    setupClickAction(btn2, std::move(btn2Cb));
    setupScrollActions(btn1);
    setupScrollActions(btn2);
    setupNavigation(btn1, btn2);

    // 创建对话框
    auto* dlg = new CustomDialog(content);
    if (bAction) dlg->onB(std::move(bAction));
    dlg->open();
    if (btn1) brls::Application::giveFocus(btn1);
}

void close(std::function<void()> cb) {
    s_scroll = nullptr;
    s_atBoundary = 0;
    CustomDialog::close(std::move(cb));
}

} // namespace ScrollDialog
