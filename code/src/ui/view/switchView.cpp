/**
 * SwitchView - 开关控件实现
 */

#include "ui/view/switchView.hpp"

SwitchView::SwitchView() {
    setWidth(TRACK_WIDTH);
    setHeight(TRACK_HEIGHT);
    setFocusable(false);
}

void SwitchView::setOn(bool on, bool animated) {
    m_on = on;
    float target = on ? 1.0f : 0.0f;
    if (animated) {
        brls::Style style = brls::Application::getStyle();
        m_knobProgress.reset();
        m_knobProgress.addStep(target, style["brls/animations/highlight"], brls::EasingFunction::quadraticOut);
        m_knobProgress.start();
    } else {
        m_knobProgress.reset(target);
    }
}

bool SwitchView::isAnimating() {
    return m_knobProgress.isRunning();
}

void SwitchView::setEnabled(bool enabled) {
    m_enabled = enabled;
}

void SwitchView::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) {
    float progress = m_knobProgress.getValue();

    auto theme = brls::Application::getTheme();
    NVGcolor offColor = theme["app/progressTrack"];
    NVGcolor onColor = theme["app/textHighlight"];
    NVGcolor trackColor = nvgLerpRGBA(offColor, onColor, progress);
    if (!m_enabled) trackColor.a *= 0.4f;

    // 轨道
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, width, height, height * 0.5f);
    nvgFillColor(vg, trackColor);
    nvgFill(vg);

    // 滑块
    float knobX = x + KNOB_PADDING + progress * (width - KNOB_SIZE - KNOB_PADDING * 2.0f);
    float knobY = y + (height - KNOB_SIZE) * 0.5f;
    NVGcolor knobColor = theme["app/switchKnob"];
    if (!m_enabled) knobColor.a *= 0.55f;
    nvgBeginPath(vg);
    nvgCircle(vg, knobX + KNOB_SIZE * 0.5f, knobY + KNOB_SIZE * 0.5f, KNOB_SIZE * 0.5f);
    nvgFillColor(vg, knobColor);
    nvgFill(vg);

    // 滑块描边（轻微立体感）
    NVGcolor knobBorder = theme["app/switchKnobBorder"];
    if (!m_enabled) knobBorder.a *= 0.5f;
    nvgBeginPath(vg);
    nvgCircle(vg, knobX + KNOB_SIZE * 0.5f, knobY + KNOB_SIZE * 0.5f, KNOB_SIZE * 0.5f);
    nvgStrokeColor(vg, knobBorder);
    nvgStrokeWidth(vg, 1.0f);
    nvgStroke(vg);
}
