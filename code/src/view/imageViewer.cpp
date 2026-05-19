/**
 * ImageViewer - 图片浏览组件实现
 *
 * XML 布局 + 动态指示器。
 * 全屏暗色遮罩 + 居中大图，B/A/触摸关闭，左右键切换。
 * 切换图片时使用双 Image 平移滑动动画。
 */

#include "view/imageViewer.hpp"
#include <borealis/core/i18n.hpp>
#include "core/audio.hpp"
#include "utils/pageNav.hpp"

static constexpr int SLIDE_DURATION = 400;

ImageViewer* ImageViewer::s_current = nullptr;

ImageViewer::ImageViewer(const std::vector<int>& textureIds, int startIndex)
    : m_textureIds(textureIds), m_currentIndex(startIndex)
{
    this->inflateFromXMLRes("xml/view/imageViewer.xml");
    setBackgroundColor(brls::Application::getTheme()["brls/backdrop"]);

    m_image->setFreeTexture(false);
    m_imageNext->setFreeTexture(false);
    m_image->innerSetImage(m_textureIds[m_currentIndex]);

    setupIndicator();
    setupActions();
}

void ImageViewer::setupIndicator()
{
    if (m_textureIds.size() <= 1) return;

    m_indicator->setVisibility(brls::Visibility::VISIBLE);

    auto* pill = new brls::Box();
    pill->setAxis(brls::Axis::ROW);
    pill->setAlignItems(brls::AlignItems::CENTER);
    pill->setBackgroundColor(brls::Application::getTheme()["app/tagBg"]);
    pill->setCornerRadius(12);
    pill->setPaddingTop(6);
    pill->setPaddingBottom(6);
    pill->setPaddingLeft(10);
    pill->setPaddingRight(10);
    m_indicator->addView(pill);

    for (size_t i = 0; i < m_textureIds.size(); i++) {
        auto* dot = new brls::Label();
        dot->setText("●");
        dot->setFontSize(10);
        dot->setMarginLeft(i > 0 ? 8 : 0);
        m_dots.push_back(dot);
        pill->addView(dot);
    }
    updateIndicator();
}

void ImageViewer::setupActions()
{
    registerAction(brls::getStr("views/imageViewer/back"), brls::BUTTON_B, [](brls::View*) {
        Audio::instance()->play(SoundEffect::Enter);
        ImageViewer::close();
        return true;
    });

    registerAction(brls::getStr("views/imageViewer/confirm"), brls::BUTTON_A, [](brls::View*) {
        Audio::instance()->play(SoundEffect::Enter);
        ImageViewer::close();
        return true;
    });

    registerAction("", brls::BUTTON_NAV_LEFT, [this](brls::View*) {
        navigate(false);
        return true;
    }, true);

    registerAction("", brls::BUTTON_NAV_RIGHT, [this](brls::View*) {
        navigate(true);
        return true;
    }, true);

    brls::TapGestureConfig tapConfig;
    tapConfig.highlightOnSelect = false;
    addGestureRecognizer(new brls::TapGestureRecognizer(this, tapConfig));
    addGestureRecognizer(new brls::PanGestureRecognizer([this](brls::PanGestureStatus status, brls::Sound* soundToPlay) {
        if (status.state != brls::GestureState::END) return;
        float deltaX = status.position.x - status.startPosition.x;
        if (deltaX < -50) navigate(true);
        else if (deltaX > 50) navigate(false);
    }, brls::PanAxis::HORIZONTAL));
}

void ImageViewer::navigate(bool right)
{
    if (m_animating) return;
    int count = (int)m_textureIds.size();
    if (count > 1) {
        Audio::instance()->play(SoundEffect::Enter);
        int next = right ? (m_currentIndex + 1) % count : (m_currentIndex - 1 + count) % count;
        switchImage(next, right);
    } else {
        Audio::instance()->play(SoundEffect::FocusLimit);
        shakeImage(right);
    }
}

void ImageViewer::switchImage(int index, bool slideLeft)
{
    float width = getWidth();
    float exitTarget  = slideLeft ? -width : width;
    float enterStart  = slideLeft ?  width : -width;

    m_imageNext->innerSetImage(m_textureIds[index]);
    m_imageNext->setVisibility(brls::Visibility::VISIBLE);
    m_imageNext->setTranslationX(enterStart);

    m_animating = true;
    m_currentIndex = index;
    updateIndicator();

    // 当前图滑出
    m_offsetCur.stop();
    m_offsetCur.reset(0.0f);
    m_offsetCur.addStep(exitTarget, SLIDE_DURATION, brls::EasingFunction::cubicOut);
    m_offsetCur.setTickCallback([this] {
        m_image->setTranslationX(m_offsetCur);
    });
    m_offsetCur.start();

    // 新图滑入
    m_offsetNext.stop();
    m_offsetNext.reset(enterStart);
    m_offsetNext.addStep(0.0f, SLIDE_DURATION, brls::EasingFunction::cubicOut);
    m_offsetNext.setTickCallback([this] {
        m_imageNext->setTranslationX(m_offsetNext);
    });
    m_offsetNext.setEndCallback([this](bool) {
        m_image->innerSetImage(m_textureIds[m_currentIndex]);
        m_image->setTranslationX(0);
        m_imageNext->setVisibility(brls::Visibility::INVISIBLE);
        m_imageNext->setTranslationX(0);
        m_animating = false;
    });
    m_offsetNext.start();
}

void ImageViewer::shakeImage(bool right)
{
    m_animating = true;
    float offset = right ? 20.0f : -20.0f;

    m_offsetCur.stop();
    m_offsetCur.reset(0.0f);
    m_offsetCur.addStep(offset, 80, brls::EasingFunction::cubicOut);
    m_offsetCur.addStep(0.0f, 150, brls::EasingFunction::cubicOut);
    m_offsetCur.setTickCallback([this] {
        m_image->setTranslationX(m_offsetCur);
    });
    m_offsetCur.setEndCallback([this](bool) {
        m_image->setTranslationX(0);
        m_animating = false;
    });
    m_offsetCur.start();
}

void ImageViewer::updateIndicator()
{
    for (size_t i = 0; i < m_dots.size(); i++) {
        if ((int)i == m_currentIndex) m_dots[i]->setTextColor(nvgRGBA(255, 255, 255, 255));
        else m_dots[i]->setTextColor(nvgRGBA(255, 255, 255, 80));
    }
}

brls::View* ImageViewer::getDefaultFocus()
{
    return m_image;
}

brls::AppletFrame* ImageViewer::getAppletFrame()
{
    return nullptr;
}

void ImageViewer::open(const std::vector<int>& textureIds, int startIndex)
{
    if (s_current || textureIds.empty()) return;
    auto* viewer = new ImageViewer(textureIds, startIndex);
    s_current = viewer;
    pageNav::push(new brls::Activity(viewer));
}

void ImageViewer::close()
{
    if (!s_current) return;
    s_current = nullptr;
    brls::Application::popActivity();
}
