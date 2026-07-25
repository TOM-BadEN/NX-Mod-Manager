/**
 * GlobalHeader - 全局常驻顶部栏
 */

#include "ui/view/shell/globalHeader.hpp"
#include "common/settings.hpp"
#include "core/device.hpp"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

GlobalHeader::GlobalHeader() {
    inflateFromXMLRes("xml/view/shell/globalHeader.xml");

    brls::Application::setFPSStatus(true);
    setShowFps(Settings::getBool("UI", "showFps", false));
    setShowMem(Settings::getBool("UI", "showMem", false));
}

void GlobalHeader::setTitle(std::string title) {
    m_titleLabel->setText(title);
}

void GlobalHeader::setSubtitle(std::string subtitle) {
    if (subtitle.empty()) m_subtitleLabel->setVisibility(brls::Visibility::GONE);
    else {
        m_subtitleLabel->setText(subtitle);
        m_subtitleLabel->setVisibility(brls::Visibility::VISIBLE);
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
