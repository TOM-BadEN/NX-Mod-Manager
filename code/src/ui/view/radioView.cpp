/**
 * RadioView - 单选指示器实现
 */

#include "ui/view/radioView.hpp"

#include <algorithm>

RadioView::RadioView() {
    setWidth(SIZE);
    setHeight(SIZE);
    setFocusable(false);
}

void RadioView::setSelected(bool selected, bool animated) {
    if (animated) {
        brls::Style style = brls::Application::getStyle();
        m_progress.reset();
        m_progress.addStep(selected ? 1.0f : 0.0f, style["brls/animations/highlight"], brls::EasingFunction::quadraticOut);
        m_progress.start();
    } else {
        m_progress.reset(selected ? 1.0f : 0.0f);
    }
}

void RadioView::setEnabled(bool enabled) {
    m_enabled = enabled;
}

void RadioView::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) {
    float progress = m_progress.getValue();

    auto theme = brls::Application::getTheme();
    NVGcolor offColor = theme["app/radioRing"];
    NVGcolor onColor = theme["app/textHighlight"];
    NVGcolor ringColor = nvgLerpRGBA(offColor, onColor, progress);
    if (!m_enabled) ringColor.a *= 0.4f;

    float cx = x + width * 0.5f;
    float cy = y + height * 0.5f;
    float ringRadius = std::min(width, height) * 0.5f - RING_STROKE * 0.5f;

    // 外圈
    nvgBeginPath(vg);
    nvgCircle(vg, cx, cy, ringRadius);
    nvgStrokeColor(vg, ringColor);
    nvgStrokeWidth(vg, RING_STROKE);
    nvgStroke(vg);

    // 内圆点（随进度生长）
    if (progress > 0.0f) {
        NVGcolor dotColor = onColor;
        if (!m_enabled) dotColor.a *= 0.4f;
        float dotRadius = progress * DOT_RADIUS;
        nvgBeginPath(vg);
        nvgCircle(vg, cx, cy, dotRadius);
        nvgFillColor(vg, dotColor);
        nvgFill(vg);
    }
}
