/**
 * ImageViewer - 图片浏览组件实现
 *
 * XML 布局 + 动态指示器。
 * 全屏暗色遮罩 + 居中大图，B/A/触摸关闭，左右键切换。
 * 切换图片时使用双 Image 平移滑动动画。
 */

#include "ui/view/imageViewer.hpp"
#include "core/audio.hpp"
#include <algorithm>

ImageViewer* ImageViewer::s_current = nullptr;

ImageViewer::ImageViewer(const std::vector<int>& textureIds, const std::vector<brls::Image*>& sourceImages, int startIndex)
    : m_textureIds(textureIds), m_sourceImages(sourceImages), m_currentIndex(startIndex) {
    inflateFromXMLRes("xml/view/imageViewer.xml");
    setHideClickAnimation(true); // 避免 A 键关闭时触发全屏点击脉冲
    m_backdropColor = brls::Application::getTheme()["brls/backdrop"];

    auto transparentBackdrop = m_backdropColor;
    transparentBackdrop.a = 0.0f;
    setBackgroundColor(transparentBackdrop);

    m_image->setFreeTexture(false);
    m_imageNext->setFreeTexture(false);
    setImageTextureAndFitSize(m_image, m_textureIds[m_currentIndex]);

    m_transitionProgress.setTickCallback([this] { updateTransition(); });
    m_transitionProgress.setEndCallback([this](bool finished) { finishTransition(finished); });

    setupIndicator();
    setupActions();
}

void ImageViewer::setupIndicator() {
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

void ImageViewer::setupActions() {
    auto closeAction = [this](brls::View*) {
        if (m_state != ViewerState::Idle) return true;
        Audio::instance()->play(SoundEffect::Enter);
        ImageViewer::close();
        return true;
    };

    registerAction("", brls::BUTTON_B, closeAction, true);
    registerAction("", brls::BUTTON_A, closeAction, true);

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

void ImageViewer::navigate(bool right) {
    if (m_state != ViewerState::Idle) return;
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

void ImageViewer::switchImage(int index, bool slideLeft) {
    float width = getWidth();
    float exitTarget = slideLeft ? -width : width;
    float enterStart = slideLeft ? width : -width;

    setImageTextureAndFitSize(m_imageNext, m_textureIds[index]);
    m_imageNext->setVisibility(brls::Visibility::VISIBLE);
    m_imageNext->setTranslationX(enterStart);

    m_state = ViewerState::Navigating;
    m_currentIndex = index;
    updateIndicator();

    // 当前图滑出
    m_offsetCur.reset(0.0f);
    m_offsetCur.addStep(exitTarget, SLIDE_DURATION, brls::EasingFunction::cubicOut);
    m_offsetCur.setTickCallback([this] {
        m_image->setTranslationX(m_offsetCur);
    });
    m_offsetCur.start();

    // 新图滑入
    m_offsetNext.reset(enterStart);
    m_offsetNext.addStep(0.0f, SLIDE_DURATION, brls::EasingFunction::cubicOut);
    m_offsetNext.setTickCallback([this] {
        m_imageNext->setTranslationX(m_offsetNext);
    });
    m_offsetNext.setEndCallback([this](bool) {
        setImageTextureAndFitSize(m_image, m_textureIds[m_currentIndex]);
        m_image->setTranslationX(0);
        m_imageNext->setVisibility(brls::Visibility::INVISIBLE);
        m_imageNext->setTranslationX(0);
        m_state = ViewerState::Idle;
    });
    m_offsetNext.start();
}

void ImageViewer::setImageTextureAndFitSize(brls::Image* image, int textureId) {
    image->innerSetImage(textureId);

    float imageWidth = image->getOriginalImageWidth();
    float imageHeight = image->getOriginalImageHeight();
    if (imageWidth <= 0.0f || imageHeight <= 0.0f) return;

    float contentWidth = brls::Application::contentWidth > 0.0f
        ? brls::Application::contentWidth
        : (float)brls::Application::ORIGINAL_WINDOW_WIDTH;
    float contentHeight = brls::Application::contentHeight > 0.0f
        ? brls::Application::contentHeight
        : (float)brls::Application::ORIGINAL_WINDOW_HEIGHT;

    float maxWidth = contentWidth * IMAGE_MAX_RATIO;
    float maxHeight = contentHeight * IMAGE_MAX_RATIO;
    float scale = std::min(maxWidth / imageWidth, maxHeight / imageHeight);

    image->setDimensions(imageWidth * scale, imageHeight * scale);
}

void ImageViewer::startOpenTransition() {
    m_sourceFrame = m_sourceImages[m_currentIndex]->getFrame();
    m_targetFrame = m_image->getFrame();
    m_sourceImages[m_currentIndex]->setAlpha(0.0f);
    m_state = ViewerState::Opening;

    m_transitionProgress.reset(0.0f);
    m_transitionProgress.addStep(1.0f, OPEN_DURATION, brls::EasingFunction::cubicOut);
    updateTransition();
    m_transitionProgress.start();
}

void ImageViewer::startCloseTransition() {
    m_sourceFrame = m_sourceImages[m_currentIndex]->getFrame();
    m_targetFrame = m_image->getFrame();
    m_sourceImages[m_currentIndex]->setAlpha(0.0f);
    m_state = ViewerState::Closing;

    m_transitionProgress.reset(0.0f);
    m_transitionProgress.addStep(1.0f, CLOSE_DURATION, brls::EasingFunction::cubicOut);
    updateTransition();
    m_transitionProgress.start();
}

void ImageViewer::updateTransition() {
    float progress = m_transitionProgress.getValue();
    float backdropProgress = m_state == ViewerState::Closing ? 1.0f - progress : progress;
    auto backdrop = m_backdropColor;
    backdrop.a *= backdropProgress;
    setBackgroundColor(backdrop);
}

void ImageViewer::finishTransition(bool finished) {
    if (!finished) return;

    if (m_state == ViewerState::Opening) {
        m_sourceImages[m_currentIndex]->setAlpha(1.0f);
        setBackgroundColor(m_backdropColor);
        m_state = ViewerState::Idle;
        return;
    }

    if (m_state != ViewerState::Closing) return;

    auto* sourceImage = m_sourceImages[m_currentIndex];
    sourceImage->setAlpha(1.0f);
    s_current = nullptr;
    brls::Application::popActivity(brls::TransitionAnimation::NONE, [sourceImage] {
        brls::Application::giveFocus(sourceImage);
    });
}

void ImageViewer::shakeImage(bool right) {
    m_state = ViewerState::Navigating;
    float offset = right ? 20.0f : -20.0f;

    m_offsetCur.reset(0.0f);
    m_offsetCur.addStep(offset, 80, brls::EasingFunction::cubicOut);
    m_offsetCur.addStep(0.0f, 150, brls::EasingFunction::cubicOut);
    m_offsetCur.setTickCallback([this] {
        m_image->setTranslationX(m_offsetCur);
    });
    m_offsetCur.setEndCallback([this](bool) {
        m_image->setTranslationX(0);
        m_state = ViewerState::Idle;
    });
    m_offsetCur.start();
}

void ImageViewer::updateIndicator() {
    for (size_t i = 0; i < m_dots.size(); i++) {
        if ((int)i == m_currentIndex) m_dots[i]->setTextColor(nvgRGBA(255, 255, 255, 255));
        else m_dots[i]->setTextColor(nvgRGBA(255, 255, 255, 80));
    }
}

brls::View* ImageViewer::getDefaultFocus() {
    return m_image;
}

brls::AppletFrame* ImageViewer::getAppletFrame() {
    return nullptr;
}

void ImageViewer::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) {
    if (m_state != ViewerState::Opening && m_state != ViewerState::Closing) {
        brls::Box::draw(vg, x, y, width, height, style, ctx);
        return;
    }

    float progress = m_transitionProgress.getValue();
    float visibleProgress = m_state == ViewerState::Closing ? 1.0f - progress : progress;
    float scale = m_sourceFrame.getWidth() / m_targetFrame.getWidth();
    scale += (1.0f - scale) * visibleProgress;

    float centerX = m_sourceFrame.getMidX() + (m_targetFrame.getMidX() - m_sourceFrame.getMidX()) * visibleProgress;
    float centerY = m_sourceFrame.getMidY() + (m_targetFrame.getMidY() - m_sourceFrame.getMidY()) * visibleProgress;

    float cornerRadius = m_image->getCornerRadius();
    m_image->setCornerRadius(cornerRadius / scale);

    nvgSave(vg);
    nvgTranslate(vg, centerX, centerY);
    nvgScale(vg, scale, scale);
    nvgTranslate(vg, -m_targetFrame.getMidX(), -m_targetFrame.getMidY());
    m_image->frame(ctx);
    nvgRestore(vg);

    m_image->setCornerRadius(cornerRadius);
}

void ImageViewer::open(const std::vector<int>& textureIds, const std::vector<brls::Image*>& sourceImages, int startIndex) {
    if (s_current || textureIds.empty()) return;
    auto* viewer = new ImageViewer(textureIds, sourceImages, startIndex);
    s_current = viewer;
    brls::Application::pushActivity(new brls::Activity(viewer), brls::TransitionAnimation::NONE);
    viewer->startOpenTransition();
}

void ImageViewer::close() {
    if (!s_current || s_current->m_state != ViewerState::Idle) return;
    s_current->startCloseTransition();
}
