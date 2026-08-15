/**
 * ScrollDialog - 可滚动长文本对话框实现
 *
 * 基于 CustomDialog(Box*) 容器，不使用其自带按钮。
 * 滚动区域和按钮均定义在 scrollDialog.xml 中。
 */

#include "ui/view/dialog/scrollDialog.hpp"
#include "core/audio.hpp"

namespace {

class ScrollDialogContent : public brls::Box {
public:
    /**
     * @brief 绑定内容滚动区域
     * @param scroll 滚动区域
     */
    void setScrollingFrame(brls::ScrollingFrame* scroll) { m_scroll = scroll; }

    /**
     * @brief 尝试滚动内容，并在到达边界时播放提示和抖动高亮
     * @param view 当前接收按键的 View
     * @param delta 垂直滚动距离
     * @param dir 滚动方向
     * @return 始终返回 true，表示按键已处理
     */
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

    /** @brief 重置滚动边界状态 */
    void resetBoundary() { m_atBoundary = 0; }

    /**
     * @brief 更新按键释放后的滚动边界状态
     * @param ctx 当前帧上下文
     */
    void frame(brls::FrameContext* ctx) override {
        brls::Box::frame(ctx);

        const auto& buttons = brls::Application::getControllerState().buttons;
        if (m_atBoundary == 1 && !buttons[brls::BUTTON_NAV_UP]) m_atBoundary = 0;
        else if (m_atBoundary == 2 && !buttons[brls::BUTTON_NAV_DOWN]) m_atBoundary = 0;
    }

    /** @brief XML View 工厂函数 */
    static brls::View* create() { return new ScrollDialogContent(); }

private:
    /** @brief 检查滚动区域是否位于顶部 */
    bool isAtTop() {
        return m_scroll->getContentOffsetY() <= 0;
    }

    /** @brief 检查滚动区域是否位于底部 */
    bool isAtBottom() {
        auto& children = m_scroll->getChildren();
        float contentH = children.empty() ? 0 : children[0]->getHeight();
        float bottomLimit = contentH - m_scroll->getHeight();
        return m_scroll->getContentOffsetY() >= bottomLimit - 0.01f;
    }

    brls::ScrollingFrame* m_scroll = nullptr; // 内容滚动区域
    int m_atBoundary = 0;                    // 0=未到边界, 1=上, 2=下
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
    ScrollDialogContent* content = nullptr; // 对话框根内容
    brls::Box* body = nullptr;              // 自定义内容容器
    brls::Label* label = nullptr;           // 长文本标签
    DialogButton* btn1 = nullptr;           // 左侧按钮
    DialogButton* btn2 = nullptr;           // 右侧按钮
};

DialogViews createDialogViews() {
    static bool registered = false;
    if (!registered) {
        brls::Application::registerXMLView("ScrollDialogContent", ScrollDialogContent::create);
        registered = true;
    }

    auto* content = dynamic_cast<ScrollDialogContent*>(brls::View::createFromXMLResource("view/dialog/scrollDialog.xml"));
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

void setupAndOpen(DialogViews views, const std::string& btn1Label, std::function<void()> btn1Cb, const std::string& btn2Label, std::function<void()> btn2Cb, std::function<void()> bAction)
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

} // namespace

// ── 公开 API ──────────────────────────────────────────

namespace ScrollDialog {

void show(const std::string& text, const std::string& btn1Label, std::function<void()> btn1Cb, const std::string& btn2Label, std::function<void()> btn2Cb, std::function<void()> bAction)
{
    auto views = createDialogViews();
    if (!views.content) return;
    if (views.label) views.label->setText(text);
    setupAndOpen(views, btn1Label, std::move(btn1Cb), btn2Label, std::move(btn2Cb), std::move(bAction));
}

void show(brls::Box* body, const std::string& btn1Label, std::function<void()> btn1Cb, const std::string& btn2Label, std::function<void()> btn2Cb, std::function<void()> bAction)
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
