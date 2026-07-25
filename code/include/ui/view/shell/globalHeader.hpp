/**
 * GlobalHeader - 全局常驻顶部栏
 *
 * 只负责标题和全局设备状态的显示，不持有 Page、
 * PageHost 或 AppShell 指针。
 */

#pragma once

#include <borealis.hpp>
#include <string>

class GlobalHeader : public brls::Box {
public:
    GlobalHeader();

    /**
     * @brief 设置顶部栏标题
     * @param title 标题文本
     */
    void setTitle(std::string title);

    /**
     * @brief 设置顶部栏副标题
     * @param subtitle 副标题文本，空字符串表示不显示
     */
    void setSubtitle(std::string subtitle);

    /**
     * @brief 设置是否显示 FPS
     * @param show 是否显示
     */
    void setShowFps(bool show);

    /**
     * @brief 设置是否显示内存使用情况
     * @param show 是否显示
     */
    void setShowMem(bool show);

    /** @brief 更新全局状态并绘制顶部栏 */
    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) override;

    /** @brief XML View 工厂函数 */
    static brls::View* create();

private:
    // XML 绑定的组件
    BRLS_BIND(brls::Label, m_titleLabel, "globalHeader/title");
    BRLS_BIND(brls::Label, m_subtitleLabel, "globalHeader/subtitle");
    BRLS_BIND(brls::Label, m_fpsLabel, "globalHeader/fps");
    BRLS_BIND(brls::Label, m_memLabel, "globalHeader/memory");
    BRLS_BIND(brls::Label, m_timeLabel, "globalHeader/time");
    BRLS_BIND(brls::Label, m_batteryPercentLabel, "globalHeader/batteryPercent");

    // 状态刷新间隔
    static constexpr brls::Time TIME_UPDATE_INTERVAL_US = 1000000;      // 时间刷新间隔（1s）
    static constexpr brls::Time BATTERY_UPDATE_INTERVAL_US = 5000000;   // 电池百分比刷新间隔（5s）
    static constexpr brls::Time FPS_UPDATE_INTERVAL_US = 1000000;       // FPS 文本刷新间隔（1s）
    static constexpr brls::Time MEM_UPDATE_INTERVAL_US = 1000000;       // 内存采样间隔（1s）

    // 时间状态
    std::string m_timeText;
    brls::Time m_lastTimeUpdateUs = 0;

    // 电池百分比状态
    brls::Time m_lastBatteryUpdateUs = 0;

    // FPS 状态
    brls::Time m_lastFpsUpdateUs = 0;
    size_t m_lastFps = 0;
    bool m_showFps = false;

    // 内存状态
    brls::Time m_lastMemUpdateUs = 0;
    uint64_t m_lastUsedMB = 0;
    uint64_t m_lastTotalMB = 0;
    bool m_showMem = false;

    /** @brief 更新顶部栏状态文本 */
    void updateStatusText();

    /** @brief 更新右上角时间 */
    void updateTimeStatus(brls::Time now);

    /** @brief 更新右上角电池百分比 */
    void updateBatteryStatus(brls::Time now);

    /** @brief 更新右上角 FPS 文本 */
    void updateFpsStatus(brls::Time now);

    /** @brief 更新右上角内存文本 */
    void updateMemStatus(brls::Time now);
};
