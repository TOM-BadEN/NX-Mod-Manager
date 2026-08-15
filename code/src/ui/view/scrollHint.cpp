/**
 * ScrollHint - 滚动内容底部提示遮罩实现
 */

#include "ui/view/scrollHint.hpp"

ScrollHint::ScrollHint() {
    setCornerRadius(DEFAULT_CORNER_RADIUS);
    registerFloatXMLAttribute("solidBottomHeight", [this](float height) { setSolidBottomHeight(height); });
}

void ScrollHint::setSolidBottomHeight(float height) {
    m_solidBottomHeight = height;
}

void ScrollHint::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) {
    nvgSave(vg);

    NVGcolor gradientEnd = ctx->theme["app/scrollHintBg"];
    NVGcolor gradientStart = gradientEnd;
    gradientStart.a = 0.0f;

    NVGpaint gradient = nvgLinearGradient(vg, x, y, x, y + height - m_solidBottomHeight, gradientStart, gradientEnd);
    nvgBeginPath(vg);
    nvgRoundedRectVarying(vg, x, y, width, height, 0.0f, 0.0f, getCornerRadius(), getCornerRadius());
    nvgFillPaint(vg, a(gradient));
    nvgFill(vg);

    nvgRestore(vg);
    brls::Box::draw(vg, x, y, width, height, style, ctx);
}

brls::View* ScrollHint::create() {
    return new ScrollHint();
}
