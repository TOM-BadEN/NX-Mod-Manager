/**
 * GlobalHeader - 全局常驻顶部栏
 *
 * 只负责标题和全局设备状态的显示，不持有 Page、
 * PageHost 或 AppShell 指针。
 */

#pragma once

#include <borealis.hpp>
#include "ui/core/headerState.hpp"
#include "ui/view/gradientBox.hpp"
#include <optional>
#include <string>

class GlobalHeader : public brls::Box {
public:
    GlobalHeader();

    /**
     * @brief 设置左侧标题区域的完整显示状态
     * @param state 导航标题、普通标题和内容标题状态
     */
    void setHeaderState(const HeaderState& state);

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

    /**
     * @brief 播放导航高亮边界抖动动画
     * @param right 是否向右抖动，false 表示向左
     */
    void shakeHeaderNav(bool right);

    /** @brief 更新全局状态并绘制顶部栏 */
    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) override;

    /** @brief XML View 工厂函数 */
    static brls::View* create();

private:
    // XML 绑定的组件
    BRLS_BIND(brls::Box, m_navigationCapsule, "globalHeader/navigationCapsule");     // 导航标题胶囊
    BRLS_BIND(brls::Box, m_navigationContent, "globalHeader/navigationContent");     // 导航标题内容容器
    BRLS_BIND(brls::Box, m_titleCapsule, "globalHeader/titleCapsule");               // 普通标题胶囊
    BRLS_BIND(brls::Box, m_titleContent, "globalHeader/titleContent");               // 普通标题内容容器
    BRLS_BIND(brls::Box, m_contentTitleCapsule, "globalHeader/contentTitleCapsule"); // 内容标题胶囊
    BRLS_BIND(brls::Label, m_contentTitleLabel, "globalHeader/contentTitle");        // 内容标题文字
    BRLS_BIND(brls::Label, m_fpsLabel, "globalHeader/fps");                          // FPS 文本
    BRLS_BIND(brls::Label, m_memLabel, "globalHeader/memory");                       // 内存使用文本
    BRLS_BIND(brls::Label, m_timeLabel, "globalHeader/time");                        // 当前时间文本
    BRLS_BIND(brls::Label, m_batteryPercentLabel, "globalHeader/batteryPercent");    // 电池百分比文本

    // 左侧标题区域状态
    HeaderState m_headerState;                    // 当前完整标题状态
    GradientBox* m_navigationHighlight = nullptr; // 当前导航组唯一的渐变高亮层，由 m_navigationContent 间接持有
    brls::Animatable m_navigationHighlightX{0.0f}; // 导航高亮横向动画值

    // 导航页面布局
    static constexpr float NAVIGATION_PAGE_WIDTH = 64.0f;  // 单个导航页面的宽度
    static constexpr float NAVIGATION_PAGE_SPACING = 7.0f; // 相邻导航页面之间的间距
    static constexpr float TITLE_CAPSULE_SPACING = 10.0f;   // 相邻标题胶囊之间的间距
    static constexpr float NAVIGATION_SHAKE_DISTANCE = 12.0f; // 导航高亮边界抖动距离
    static constexpr int NAVIGATION_ANIM_DURATION = 300;    // 导航高亮平移动画持续时间（毫秒）
    static constexpr int NAVIGATION_SHAKE_STEP_DURATION = 50; // 导航高亮单段抖动持续时间（毫秒）

    // 状态刷新间隔
    static constexpr brls::Time TIME_UPDATE_INTERVAL_US = 1000000;    // 时间刷新间隔（1s）
    static constexpr brls::Time BATTERY_UPDATE_INTERVAL_US = 5000000; // 电池百分比刷新间隔（5s）
    static constexpr brls::Time FPS_UPDATE_INTERVAL_US = 1000000;     // FPS 文本刷新间隔（1s）
    static constexpr brls::Time MEM_UPDATE_INTERVAL_US = 1000000;     // 内存采样间隔（1s）

    // 时间状态
    std::string m_timeText;            // 当前显示的时间文本
    brls::Time m_lastTimeUpdateUs = 0; // 上次刷新时间文本的时间点

    // 电池百分比状态
    brls::Time m_lastBatteryUpdateUs = 0; // 上次刷新电池百分比的时间点

    // FPS 状态
    brls::Time m_lastFpsUpdateUs = 0; // 上次刷新 FPS 文本的时间点
    size_t m_lastFps = 0;             // 上次显示的 FPS 数值
    bool m_showFps = false;           // 是否显示 FPS

    // 内存状态
    brls::Time m_lastMemUpdateUs = 0; // 上次刷新内存文本的时间点
    uint64_t m_lastUsedMB = 0;        // 上次显示的已用内存
    uint64_t m_lastTotalMB = 0;       // 上次显示的总内存
    bool m_showMem = false;           // 是否显示内存信息

    /** @brief 应用普通标题状态 */
    void applyTitleState(const std::optional<TitleState>& state);

    /** @brief 应用导航标题状态 */
    void applyNavigationState(const std::optional<NavigationState>& state);

    /** @brief 应用内容标题状态 */
    void applyContentTitleState(const std::optional<std::string>& state);

    /** @brief 判断两次导航状态是否属于同一个导航组 */
    bool hasSameNavigationStructure(const NavigationState& current, const NavigationState& next) const;

    /**
     * @brief 更新导航组中当前页面的高亮状态
     * @param selectedPageIndex 当前页面索引
     * @param animated 是否播放平移动画
     */
    void updateNavigationSelection(std::size_t selectedPageIndex, bool animated);

    /** @brief 播放导航高亮边界抖动动画 */
    void shakeNavigationHighlight(float distance);

    /** @brief 更新标题胶囊之间的间距 */
    void updateTitleCapsuleSpacing();

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
