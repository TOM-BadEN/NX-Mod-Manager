/**
 * ScrollDialog - 可滚动长文本对话框实现
 *
 * 基于 CustomDialog(Box*) 容器，不使用其自带按钮。
 * 滚动区域和按钮均定义在 scrollDialog.xml 中。
 */

#include "view/scrollDialog.hpp"
#include "core/audio.hpp"

namespace {

class ScrollDialogContent : public brls::Box {
public:
    void setScrollingFrame(brls::ScrollingFrame* scroll) { m_scroll = scroll; }

    bool tryScroll(brls::View* view, float delta, brls::FocusDirection dir) {
        if (!m_scroll) return true;

        int boundaryId = dir == brls::FocusDirection::UP ? 1 : 2;
        bool atLimit = boundaryId == 1 ? isAtTop() : isAtBottom();
        if (atLimit) {
            if (m_atBoundary != boundaryId) {
                Audio::instance()->play(SoundEffect::FocusLimit);
                view->shakeHighlight(dir);
                m_atBoundary = boundaryId;
            }
            return true;
        }

        m_atBoundary = 0;
        m_scroll->setContentOffsetY(m_scroll->getContentOffsetY() + delta, true);
        return true;
    }

    void resetBoundary() { m_atBoundary = 0; }

    void frame(brls::FrameContext* ctx) override {
        brls::Box::frame(ctx);

        const auto& buttons = brls::Application::getControllerState().buttons;
        if (m_atBoundary == 1 && !buttons[brls::BUTTON_NAV_UP]) m_atBoundary = 0;
        else if (m_atBoundary == 2 && !buttons[brls::BUTTON_NAV_DOWN]) m_atBoundary = 0;
    }

    static brls::View* create() { return new ScrollDialogContent(); }

private:
    bool isAtTop() {
        return m_scroll->getContentOffsetY() <= 0;
    }

    bool isAtBottom() {
        auto& children = m_scroll->getChildren();
        float contentH = children.empty() ? 0 : children[0]->getHeight();
        float bottomLimit = contentH - m_scroll->getHeight();
        return m_scroll->getContentOffsetY() >= bottomLimit - 0.01f;
    }

    brls::ScrollingFrame* m_scroll = nullptr;
    int m_atBoundary = 0;  // 0=未到边界, 1=上, 2=下
};

// ── 按钮配置 ──────────────────────────────────────────

void setupClickAction(DialogButton* btn, std::function<void()> cb) {
    if (!btn) return;
    btn->registerClickAction([cb = std::move(cb)](brls::View*) {
        Audio::instance()->play(SoundEffect::Enter);
        cb();
        return true;
    });
}

void setupScrollActions(DialogButton* btn, ScrollDialogContent* content) {
    if (!btn) return;
    btn->registerAction("", brls::BUTTON_NAV_UP, [content](brls::View* v) {
        return content->tryScroll(v, -60.0f, brls::FocusDirection::UP);
    }, true, true);
    btn->registerAction("", brls::BUTTON_NAV_DOWN, [content](brls::View* v) {
        return content->tryScroll(v, 60.0f, brls::FocusDirection::DOWN);
    }, true, true);
}

void setupNavigation(DialogButton* btn1, DialogButton* btn2, ScrollDialogContent* content) {
    if (!btn1 || !btn2) return;

    // 左右切换焦点
    btn1->registerAction("", brls::BUTTON_NAV_RIGHT, [btn2, content](...) {
        content->resetBoundary();
        Audio::instance()->play(SoundEffect::Focus);
        brls::Application::giveFocus(btn2);
        return true;
    }, true);
    btn2->registerAction("", brls::BUTTON_NAV_LEFT, [btn1, content](...) {
        content->resetBoundary();
        Audio::instance()->play(SoundEffect::Focus);
        brls::Application::giveFocus(btn1);
        return true;
    }, true);

    // 边界抖动
    btn1->registerAction("", brls::BUTTON_NAV_LEFT, [btn1, content](...) {
        content->resetBoundary();
        Audio::instance()->play(SoundEffect::FocusLimit);
        btn1->shakeHighlight(brls::FocusDirection::LEFT);
        return true;
    }, true);
    btn2->registerAction("", brls::BUTTON_NAV_RIGHT, [btn2, content](...) {
        content->resetBoundary();
        Audio::instance()->play(SoundEffect::FocusLimit);
        btn2->shakeHighlight(brls::FocusDirection::RIGHT);
        return true;
    }, true);
}

struct DialogViews {
    ScrollDialogContent* content = nullptr;
    brls::Box* body = nullptr;
    brls::Label* label = nullptr;
    DialogButton* btn1 = nullptr;
    DialogButton* btn2 = nullptr;
};

DialogViews createDialogViews() {
    static bool registered = false;
    if (!registered) {
        brls::Application::registerXMLView("ScrollDialogContent", ScrollDialogContent::create);
        registered = true;
    }

    auto* content = dynamic_cast<ScrollDialogContent*>(
        brls::View::createFromXMLResource("view/scrollDialog.xml"));
    if (!content) return {};

    content->setScrollingFrame(dynamic_cast<brls::ScrollingFrame*>(content->getView("scroll_dialog/scroll")));
    return {
        content,
        dynamic_cast<brls::Box*>(content->getView("scroll_dialog/body")),
        dynamic_cast<brls::Label*>(content->getView("scroll_dialog/text")),
        dynamic_cast<DialogButton*>(content->getView("scroll_dialog/btn1")),
        dynamic_cast<DialogButton*>(content->getView("scroll_dialog/btn2")),
    };
}

void setupAndOpen(DialogViews views,
    const std::string& btn1Label, std::function<void()> btn1Cb,
    const std::string& btn2Label, std::function<void()> btn2Cb,
    std::function<void()> bAction)
{
    if (views.btn1) views.btn1->setText(btn1Label);
    if (views.btn2) views.btn2->setText(btn2Label);

    setupClickAction(views.btn1, std::move(btn1Cb));
    setupClickAction(views.btn2, std::move(btn2Cb));
    setupScrollActions(views.btn1, views.content);
    setupScrollActions(views.btn2, views.content);
    setupNavigation(views.btn1, views.btn2, views.content);

    auto* dlg = new CustomDialog(views.content);
    if (bAction) dlg->onB(std::move(bAction));
    dlg->open();
    if (views.btn1) brls::Application::giveFocus(views.btn1);
}

} // anonymous namespace

// ── 公开 API ──────────────────────────────────────────

namespace ScrollDialog {

void show(const std::string& text,
    const std::string& btn1Label, std::function<void()> btn1Cb,
    const std::string& btn2Label, std::function<void()> btn2Cb,
    std::function<void()> bAction)
{
    auto views = createDialogViews();
    if (!views.content) return;
    if (views.label) views.label->setText(text);
    setupAndOpen(views, btn1Label, std::move(btn1Cb), btn2Label, std::move(btn2Cb), std::move(bAction));
}

void show(brls::Box* body,
    const std::string& btn1Label, std::function<void()> btn1Cb,
    const std::string& btn2Label, std::function<void()> btn2Cb,
    std::function<void()> bAction)
{
    if (!body) return;

    auto views = createDialogViews();
    if (!views.content || !views.body) {
        delete body;
        return;
    }

    views.body->clearViews();
    views.body->addView(body);
    setupAndOpen(views, btn1Label, std::move(btn1Cb), btn2Label, std::move(btn2Cb), std::move(bAction));
}

void close(std::function<void()> cb) {
    CustomDialog::close(std::move(cb));
}

} // namespace ScrollDialog
