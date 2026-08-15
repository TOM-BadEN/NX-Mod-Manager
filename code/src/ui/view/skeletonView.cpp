/**
 * SkeletonView - 通用轻量骨架组件实现
 */

#include "ui/view/skeletonView.hpp"
#include <cmath>

SkeletonView::SkeletonView() {
    setCornerRadius(10);
}

void SkeletonView::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) {
    brls::Time currentTime = brls::getCPUTimeUsec() / 1000;
    float progress         = static_cast<float>(currentTime % 1000) / 1000.0f;
    progress               = std::fabs(0.5f - progress) + 0.25f;

    NVGcolor start = m_background;
    NVGcolor end   = m_background;
    start.a        = 0.35f;
    end.a          = progress;

    NVGpaint paint = nvgLinearGradient(vg, x, y, x + width, y + height, a(start), a(end));
    nvgBeginPath(vg);
    nvgFillPaint(vg, paint);
    nvgRoundedRect(vg, x, y, width, height, getCornerRadius());
    nvgFill(vg);
}

brls::View* SkeletonView::create() {
    return new SkeletonView();
}
