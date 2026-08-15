/**
 * PageAnim - 页面切换动画
 */

#include "ui/anim/pageAnim.hpp"
#include <utility>

PageAnim::PageAnim() {
    m_progress.setTickCallback([this] { updateViews(); });
    m_progress.setEndCallback([this](bool finished) { finish(finished); });
}

PageAnim::~PageAnim() {
    if (m_progress.isRunning()) m_progress.stop();
}

bool PageAnim::start(brls::View* entering, brls::View* leaving, PageAnimType type, float width, float height, std::function<void()> completed) {
    if (m_progress.isRunning()) return false;

    if (type == PageAnimType::None) {
        if (completed) completed();
        return true;
    }

    if (!entering || !leaving || entering == leaving || width <= 0.0f || height <= 0.0f) return false;

    switch (type) {
        case PageAnimType::SlideFromLeft:
            m_offsetX = -width;
            m_offsetY = 0.0f;
            break;
        case PageAnimType::SlideFromRight:
            m_offsetX = width;
            m_offsetY = 0.0f;
            break;
        case PageAnimType::None:
            break;
    }

    m_entering = entering;
    m_leaving = leaving;
    m_completed = std::move(completed);

    m_entering->setTranslationX(m_offsetX);
    m_entering->setTranslationY(m_offsetY);
    m_leaving->setTranslationX(0.0f);
    m_leaving->setTranslationY(0.0f);

    m_progress.reset(0.0f);
    m_progress.addStep(1.0f, ANIM_DURATION, brls::EasingFunction::cubicOut);
    m_progress.start();

    return true;
}

bool PageAnim::isRunning() {
    return m_progress.isRunning();
}

void PageAnim::updateViews() {
    if (!m_entering || !m_leaving) return;

    float progress = m_progress.getValue();
    m_entering->setTranslationX(m_offsetX * (1.0f - progress));
    m_entering->setTranslationY(m_offsetY * (1.0f - progress));
    m_leaving->setTranslationX(-m_offsetX * progress);
    m_leaving->setTranslationY(-m_offsetY * progress);
}

void PageAnim::finish(bool finished) {
    resetViews();

    auto completed = std::move(m_completed);
    m_entering = nullptr;
    m_leaving = nullptr;
    m_offsetX = 0.0f;
    m_offsetY = 0.0f;

    if (finished && completed) completed();
}

void PageAnim::resetViews() {
    if (m_entering) {
        m_entering->setTranslationX(0.0f);
        m_entering->setTranslationY(0.0f);
    }
    if (m_leaving) {
        m_leaving->setTranslationX(0.0f);
        m_leaving->setTranslationY(0.0f);
    }
}
