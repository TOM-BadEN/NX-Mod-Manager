/**
 * GlobalHeader - 全局常驻顶部栏
 */

#include "ui/view/shell/globalHeader.hpp"
#include "common/settings.hpp"
#include "core/device.hpp"
#include <borealis/core/cache_helper.hpp>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

GlobalHeader::GlobalHeader() {
    inflateFromXMLRes("xml/view/shell/globalHeader.xml");

    m_navigationHighlightX.setTickCallback([this] {
        if (m_navigationHighlight) m_navigationHighlight->setTranslationX(m_navigationHighlightX.getValue());
    });

    brls::Application::setFPSStatus(true);
    setShowFps(Settings::getBool("UI", "showFps", false));
    setShowMem(Settings::getBool("UI", "showMem", false));
}

void GlobalHeader::setHeaderState(const HeaderState& state) {
    if (m_headerState == state) return;

    if (m_headerState.navigation != state.navigation) {
        if (m_headerState.navigation && state.navigation && hasSameNavigationStructure(*m_headerState.navigation, *state.navigation)) {
            updateNavigationSelection(state.navigation->selectedPageIndex, true);
        } else {
            applyNavigationState(state.navigation);
        }
        m_headerState.navigation = state.navigation;
    }

    if (m_headerState.title != state.title) {
        applyTitleState(state.title);
        m_headerState.title = state.title;
    }

    if (m_headerState.contentTitle != state.contentTitle) {
        applyContentTitleState(state.contentTitle);
        m_headerState.contentTitle = state.contentTitle;
    }

    updateTitleCapsuleSpacing();
}

void GlobalHeader::shakeHeaderNav(bool right) {
    if (!m_headerState.navigation) return;
    shakeNavigationHighlight(right ? NAVIGATION_SHAKE_DISTANCE : -NAVIGATION_SHAKE_DISTANCE);
}

void GlobalHeader::applyTitleState(const std::optional<TitleState>& state) {
    constexpr float TITLE_ICON_SIZE = 36.0f;
    constexpr float TITLE_ICON_SPACING = 12.0f;
    constexpr float TITLE_FONT_SIZE = 25.0f;
    constexpr float SUBTITLE_FONT_SIZE = 16.0f;
    constexpr float SUBTITLE_SPACING = 12.0f;

    m_titleContent->clearViews();
    m_titleCapsule->setVisibility(state ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
    if (!state) return;

    int textureId = 0;
    if (!state->iconTextureKey.empty()) textureId = brls::TextureCache::instance().getCache(state->iconTextureKey);
    if (textureId != 0 || !state->iconPath.empty()) {
        auto* icon = new brls::Image();
        icon->setWidth(TITLE_ICON_SIZE);
        icon->setHeight(TITLE_ICON_SIZE);
        icon->setScalingType(brls::ImageScalingType::FILL);
        icon->setCornerRadius(TITLE_ICON_SIZE / 2.0f);

        if (textureId != 0) {
            icon->setFreeTexture(false);
            icon->innerSetImage(textureId);
        } else icon->setImageFromRes(state->iconPath);

        if (!state->title.empty() || !state->subtitle.empty()) icon->setMarginRight(TITLE_ICON_SPACING);

        m_titleContent->addView(icon);
    }

    auto* title = new brls::Label();
    title->setText(state->title);
    title->setFontSize(TITLE_FONT_SIZE);
    title->setSingleLine(true);
    title->setShrink(1.0f);
    m_titleContent->addView(title);

    if (!state->subtitle.empty()) {
        auto* subtitle = new brls::Label();
        subtitle->setText(state->subtitle);
        subtitle->setFontSize(SUBTITLE_FONT_SIZE);
        subtitle->setTextColor(brls::Application::getTheme()["app/textSecondary"]);
        subtitle->setSingleLine(true);
        subtitle->setShrink(0.0f);
        subtitle->setMarginLeft(SUBTITLE_SPACING);
        m_titleContent->addView(subtitle);
    }
}

void GlobalHeader::applyNavigationState(const std::optional<NavigationState>& state) {
    constexpr float BUTTON_FONT_SIZE = 19.0f;
    constexpr float BUTTON_SPACING = 11.0f;
    constexpr float PAGE_HEIGHT = 48.0f;
    constexpr float PAGE_ICON_SIZE = 32.0f;

    m_navigationHighlightX.reset();
    m_navigationHighlight = nullptr;
    m_navigationContent->clearViews();
    m_navigationCapsule->setVisibility(state ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
    if (!state) return;

    if (!state->leftButton.empty()) {
        auto* leftButton = new brls::Label();
        leftButton->setText(state->leftButton);
        leftButton->setFontSize(BUTTON_FONT_SIZE);
        leftButton->setSingleLine(true);
        leftButton->setMarginRight(BUTTON_SPACING);
        m_navigationContent->addView(leftButton);
    }

    if (!state->pageIconPaths.empty()) {
        auto* pages = new brls::Box(brls::Axis::ROW);
        pages->setWidth(NAVIGATION_PAGE_WIDTH * static_cast<float>(state->pageIconPaths.size()) + NAVIGATION_PAGE_SPACING * static_cast<float>(state->pageIconPaths.size() - 1));
        pages->setHeight(PAGE_HEIGHT);

        auto theme = brls::Application::getTheme();
        m_navigationHighlight = new GradientBox();
        m_navigationHighlight->setWidth(NAVIGATION_PAGE_WIDTH);
        m_navigationHighlight->setHeight(PAGE_HEIGHT);
        m_navigationHighlight->setPositionType(brls::PositionType::ABSOLUTE);
        m_navigationHighlight->setPositionTop(0.0f);
        m_navigationHighlight->setPositionLeft(0.0f);
        m_navigationHighlight->setCornerRadius(PAGE_HEIGHT / 2.0f);
        m_navigationHighlight->setGradientStartColor(theme["app/navigationHighlightTop"]);
        m_navigationHighlight->setGradientEndColor(theme["app/navigationHighlightBottom"]);
        m_navigationHighlight->setGradientDirection(GradientDirection::Vertical);
        m_navigationHighlight->setBorderColor(theme["brls/highlight/color2"]);
        m_navigationHighlight->setBorderThickness(1.5f);
        pages->addView(m_navigationHighlight);

        for (std::size_t i = 0; i < state->pageIconPaths.size(); i++) {
            auto* page = new brls::Box();
            page->setWidth(NAVIGATION_PAGE_WIDTH);
            page->setHeight(PAGE_HEIGHT);
            page->setAlignItems(brls::AlignItems::CENTER);
            page->setJustifyContent(brls::JustifyContent::CENTER);
            if (i > 0) page->setMarginLeft(NAVIGATION_PAGE_SPACING);

            auto* icon = new brls::Image();
            icon->setWidth(PAGE_ICON_SIZE);
            icon->setHeight(PAGE_ICON_SIZE);
            icon->setImageFromRes(state->pageIconPaths[i]);

            page->addView(icon);
            pages->addView(page);
        }

        m_navigationContent->addView(pages);
    }

    if (!state->rightButton.empty()) {
        auto* rightButton = new brls::Label();
        rightButton->setText(state->rightButton);
        rightButton->setFontSize(BUTTON_FONT_SIZE);
        rightButton->setSingleLine(true);
        rightButton->setMarginLeft(BUTTON_SPACING);
        m_navigationContent->addView(rightButton);
    }

    updateNavigationSelection(state->selectedPageIndex, false);
}

void GlobalHeader::applyContentTitleState(const std::optional<std::string>& state) {
    m_contentTitleLabel->setText(state ? *state : "");
    m_contentTitleCapsule->setVisibility(state ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
}

bool GlobalHeader::hasSameNavigationStructure(const NavigationState& current, const NavigationState& next) const {
    return current.leftButton == next.leftButton && current.pageIconPaths == next.pageIconPaths && current.rightButton == next.rightButton;
}

void GlobalHeader::updateNavigationSelection(std::size_t selectedPageIndex, bool animated) {
    if (!m_navigationHighlight) return;

    float targetX = static_cast<float>(selectedPageIndex) * (NAVIGATION_PAGE_WIDTH + NAVIGATION_PAGE_SPACING);
    if (!animated) {
        m_navigationHighlightX.reset(targetX);
        m_navigationHighlight->setTranslationX(targetX);
        return;
    }

    m_navigationHighlightX.reset(m_navigationHighlightX.getValue());
    m_navigationHighlightX.addStep(targetX, NAVIGATION_ANIM_DURATION, brls::EasingFunction::cubicOut);
    m_navigationHighlightX.start();
}

void GlobalHeader::shakeNavigationHighlight(float distance) {
    if (!m_navigationHighlight) return;

    float targetX = static_cast<float>(m_headerState.navigation->selectedPageIndex) * (NAVIGATION_PAGE_WIDTH + NAVIGATION_PAGE_SPACING);
    m_navigationHighlightX.reset(m_navigationHighlightX.getValue());
    m_navigationHighlightX.addStep(targetX + distance, NAVIGATION_SHAKE_STEP_DURATION, brls::EasingFunction::cubicOut);
    m_navigationHighlightX.addStep(targetX - distance / 2.0f, NAVIGATION_SHAKE_STEP_DURATION, brls::EasingFunction::cubicOut);
    m_navigationHighlightX.addStep(targetX + distance / 4.0f, NAVIGATION_SHAKE_STEP_DURATION, brls::EasingFunction::cubicOut);
    m_navigationHighlightX.addStep(targetX, NAVIGATION_SHAKE_STEP_DURATION, brls::EasingFunction::cubicOut);
    m_navigationHighlightX.start();
}

void GlobalHeader::updateTitleCapsuleSpacing() {
    bool hasPreviousCapsule = false;

    if (m_headerState.navigation) {
        m_navigationCapsule->setMarginLeft(0.0f);
        hasPreviousCapsule = true;
    }

    if (m_headerState.title) {
        m_titleCapsule->setMarginLeft(hasPreviousCapsule ? TITLE_CAPSULE_SPACING : 0.0f);
        hasPreviousCapsule = true;
    }

    if (m_headerState.contentTitle) {
        m_contentTitleCapsule->setMarginLeft(hasPreviousCapsule ? TITLE_CAPSULE_SPACING : 0.0f);
    }
}

void GlobalHeader::setShowFps(bool show) {
    if (m_showFps == show) return;

    m_showFps = show;
    if (show) m_lastFpsUpdateUs = 0;
    m_fpsLabel->setVisibility(show ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
}

void GlobalHeader::setShowMem(bool show) {
    if (m_showMem == show) return;

    m_showMem = show;
    if (show) m_lastMemUpdateUs = 0;
    m_memLabel->setVisibility(show ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
}

void GlobalHeader::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) {
    updateStatusText();
    brls::Box::draw(vg, x, y, width, height, style, ctx);
}

void GlobalHeader::updateStatusText() {
    brls::Time now = brls::getCPUTimeUsec();

    updateTimeStatus(now);
    updateBatteryStatus(now);
    if (m_showFps) updateFpsStatus(now);
    if (m_showMem) updateMemStatus(now);
}

void GlobalHeader::updateTimeStatus(brls::Time now) {
    if (m_lastTimeUpdateUs != 0 && now - m_lastTimeUpdateUs < TIME_UPDATE_INTERVAL_US) return;
    m_lastTimeUpdateUs = now;

    auto timeNow = std::chrono::system_clock::now();
    auto inTime = std::chrono::system_clock::to_time_t(timeNow);
    auto tm = *std::localtime(&inTime);
    std::stringstream stream;
    stream << std::put_time(&tm, "%H:%M");

    if (stream.str() == m_timeText) return;
    m_timeText = stream.str();
    m_timeLabel->setText(m_timeText);
}

void GlobalHeader::updateBatteryStatus(brls::Time now) {
    if (m_lastBatteryUpdateUs != 0 && now - m_lastBatteryUpdateUs < BATTERY_UPDATE_INTERVAL_US) return;
    m_lastBatteryUpdateUs = now;

    auto* platform = brls::Application::getPlatform();
    if (!platform->canShowBatteryLevel()) return;

    int level = platform->getBatteryLevel();
    m_batteryPercentLabel->setText(std::to_string(level) + "%");
}

void GlobalHeader::updateFpsStatus(brls::Time now) {
    if (m_lastFpsUpdateUs != 0 && now - m_lastFpsUpdateUs < FPS_UPDATE_INTERVAL_US) return;
    m_lastFpsUpdateUs = now;

    size_t fps = brls::Application::getFPS();
    if (fps == m_lastFps) return;
    m_lastFps = fps;
    m_fpsLabel->setText("FPS:" + std::to_string(fps));
}

void GlobalHeader::updateMemStatus(brls::Time now) {
    if (m_lastMemUpdateUs != 0 && now - m_lastMemUpdateUs < MEM_UPDATE_INTERVAL_US) return;
    m_lastMemUpdateUs = now;

    auto heap = deviceInfo::Memory::getHeapUsage();
    if (heap.usedMB == m_lastUsedMB && heap.totalMB == m_lastTotalMB) return;

    m_lastUsedMB = heap.usedMB;
    m_lastTotalMB = heap.totalMB;
    m_memLabel->setText(std::to_string(heap.usedMB) + " / " + std::to_string(heap.totalMB) + " MB");
}

brls::View* GlobalHeader::create() {
    return new GlobalHeader();
}
