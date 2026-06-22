/**
 * LongPressDialog - 长按确认对话框实现
 */

#include "view/longPressDialog.hpp"
#include "core/audio.hpp"
#include <borealis.hpp>
#include <borealis/core/time.hpp>
#include <algorithm>

// ── LongPressButton ───────────────────────────────────────
// DialogButton 子类，draw() 先绘蓝色填充进度，再渲染按钮本身

class LongPressButton : public DialogButton {
public:
    /**
     * @brief 设置填充进度比例
     * @param ratio 0.0（无填充）~ 1.0（全满）
     */
    void setRatio(float ratio) { m_ratio = ratio; }

    /** @brief 先绘进度填充色块，再渲染按钮本身（保证文字在上层） */
    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) override
    {
        if (m_ratio > 0.0f) {
            nvgBeginPath(vg);
            nvgRect(vg, x, y, width * m_ratio, height);
            nvgFillColor(vg, ctx->theme["app/longPressFill"]);
            nvgFill(vg);
        }
        DialogButton::draw(vg, x, y, width, height, style, ctx);
    }

    /** @brief 工厂函数 */
    static brls::View* create() { return new LongPressButton(); }

private:
    float m_ratio = 0.0f; ///< 当前填充比例（0.0~1.0）
};

// ── 内部实现 ──────────────────────────────────────────────

namespace {

brls::ScrollingFrame* s_scroll     = nullptr;
int s_atBoundary = 0;

// ── 滚动辅助（照抄 ScrollDialog）──

bool isAtTop() {
    return s_scroll->getContentOffsetY() <= 0;
}

bool isAtBottom() {
    auto& children  = s_scroll->getChildren();
    float contentH  = children.empty() ? 0 : children[0]->getHeight();
    float limit     = contentH - s_scroll->getHeight();
    return s_scroll->getContentOffsetY() >= limit - 0.01f;
}

bool tryScroll(brls::View* view, float delta, brls::FocusDirection dir, bool (*atLimit)()) {
    if (!s_scroll) return true;
    int boundaryId = (dir == brls::FocusDirection::UP) ? 1 : 2;
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

// ── LongPressContent ──────────────────────────────────────
// brls::Box 子类，override frame() 实现长按计时

class LongPressContent : public brls::Box {
public:
    /**
     * @brief 构造长按内容容器
     * @param holdSeconds 触发确认所需的持续按下时长（秒）
     * @param onConfirm   达到时长后执行的回调
     */
    LongPressContent(float holdSeconds, std::function<void()> onConfirm)
        : m_holdSeconds(holdSeconds)
        , m_onConfirm(std::move(onConfirm))
    {

    }

    /**
     * @brief 绑定进度按钮
     * @param btn 需要显示填充进度的 LongPressButton
     */
    void setup(LongPressButton* btn) {
        m_btn = btn;
    }

    /** @brief 每帧更新长按计时并同步按钮填充进度 */
    void frame(brls::FrameContext* ctx) override {
        brls::Box::frame(ctx);
        if (m_confirmed || !m_btn) return;

        brls::Time now = brls::getCPUTimeUsec();
        if (m_lastTime == 0) {
            m_lastTime = now;
            return;
        }

        float delta = static_cast<float>(now - m_lastTime) / 1000000.0f;
        m_lastTime  = now;

        const brls::ControllerState& state = brls::Application::getControllerState();
        if (state.buttons[brls::BUTTON_A]) {
            m_holdTime += delta;
            float ratio = std::min(m_holdTime / m_holdSeconds, 1.0f);
            m_btn->setRatio(ratio);

            if (m_holdTime >= m_holdSeconds) {
                m_confirmed = true;
                m_onConfirm();
            }
        } else if (m_holdTime > 0) {
            m_holdTime = 0;
            m_btn->setRatio(0.0f);
        }
    }

private:
    LongPressButton* m_btn = nullptr;        ///< 进度按钮指针
    float m_holdSeconds;                     ///< 触发确认所需时长（秒）
    float m_holdTime    = 0;                 ///< 当前累计按下时长（秒）
    bool  m_confirmed   = false;             ///< 是否已触发确认（防重入）
    brls::Time m_lastTime = 0;               ///< 上帧时间戳（微秒），用于计算 delta
    std::function<void()> m_onConfirm;       ///< 确认回调
};

} // anonymous namespace

// ── 公开 API ──────────────────────────────────────────────

namespace LongPressDialog {

void show(const std::string& text, const std::string& btnLabel, float holdSeconds, std::function<void()> onConfirm, std::function<void()> bAction)
{
    // 注册 LongPressButton（只需一次）
    static bool registered = false;
    if (!registered) {
        brls::Application::registerXMLView("LongPressButton", LongPressButton::create);
        registered = true;
    }

    // 从 XML 创建布局
    auto* box = dynamic_cast<brls::Box*>(brls::View::createFromXMLResource("view/longPressDialog.xml"));
    if (!box) return;

    // 绑定视图
    s_scroll = dynamic_cast<brls::ScrollingFrame*>(box->getView("long_press_dialog/scroll"));
    auto* label = dynamic_cast<brls::Label*>(box->getView("long_press_dialog/text"));
    auto* btn = dynamic_cast<LongPressButton*>(box->getView("long_press_dialog/btn"));
    s_atBoundary = 0;

    // 设置内容
    if (label) label->setText(text);
    if (btn) btn->setText(btnLabel);

    // 注册按钮滚动 action
    if (btn) {
        btn->registerAction("", brls::BUTTON_NAV_UP, [](brls::View* v) {
            return tryScroll(v, -60.0f, brls::FocusDirection::UP, isAtTop);
        }, true, true);
        btn->registerAction("", brls::BUTTON_NAV_DOWN, [](brls::View* v) {
            return tryScroll(v, 60.0f, brls::FocusDirection::DOWN, isAtBottom);
        }, true, true);
        btn->registerAction("", brls::BUTTON_NAV_LEFT, [btn](...) {
            s_atBoundary = 0;
            Audio::instance()->play(SoundEffect::FocusLimit);
            btn->shakeHighlight(brls::FocusDirection::LEFT);
            return true;
        }, true);
        btn->registerAction("", brls::BUTTON_NAV_RIGHT, [btn](...) {
            s_atBoundary = 0;
            Audio::instance()->play(SoundEffect::FocusLimit);
            btn->shakeHighlight(brls::FocusDirection::RIGHT);
            return true;
        }, true);
    }

    // 包裹进带 frame() 的容器
    auto* lpContent = new LongPressContent(holdSeconds, std::move(onConfirm));
    lpContent->setAxis(brls::Axis::COLUMN);
    lpContent->setWidth(brls::View::AUTO);
    lpContent->setHeight(brls::View::AUTO);
    lpContent->addView(box);
    lpContent->setup(btn);

    // 创建对话框
    auto* dlg = new CustomDialog(lpContent);
    if (bAction) dlg->onB(std::move(bAction));
    else dlg->onB([] {});   // 禁用 B 键
    dlg->open();
    if (btn) brls::Application::giveFocus(btn);
}

void close(std::function<void()> cb) {
    s_scroll = nullptr;
    s_atBoundary = 0;
    CustomDialog::close(std::move(cb));
}

} // namespace LongPressDialog
