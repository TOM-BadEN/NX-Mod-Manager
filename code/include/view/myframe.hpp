/**
 * MyFrame - 自定义应用框架
 */

#pragma once

#include <borealis.hpp>

class MyFrame : public brls::Box {
public:
    MyFrame();
    
    void handleXMLElement(tinyxml2::XMLElement* element) override;
    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) override;
    
    /**
     * @brief 设置标题
     * @param title 标题文本
     */
    void setTitle(std::string title);

    /**
     * @brief 设置副标题
     * @param text 副标题文本
     */
    void setSubtitle(const std::string& text);

    /**
     * @brief 设置图标
     * @param path 图标路径
     */
    void setIcon(std::string path);

    /**
     * @brief 设置索引文本
     * @param text 索引文本
     */
    void setIndexText(const std::string& text);

    /**
     * @brief 设置是否显示 FPS
     * @param show 是否显示
     */
    void setShowFps(bool show);

    /**
     * @brief 设置是否显示内存
     * @param show 是否显示
     */
    void setShowMem(bool show);
    
    static brls::View* create();
    
private:
    // XML 绑定的组件
    BRLS_BIND(brls::Box, m_header, "my/header");
    BRLS_BIND(brls::Box, m_contentBox, "my/content");
    BRLS_BIND(brls::Label, m_titleLabel, "my/title");
    BRLS_BIND(brls::Label, m_subtitleLabel, "my/subtitle");
    BRLS_BIND(brls::Image, m_iconImage, "my/icon");
    BRLS_BIND(brls::Label, m_fpsLabel, "my/fps");
    BRLS_BIND(brls::Label, m_memLabel, "my/memory");
    BRLS_BIND(brls::Label, m_timeLabel, "my/time");
    BRLS_BIND(brls::Label, m_batteryPercentLabel, "my/battery_percent");
    BRLS_BIND(brls::Label, m_indexLabel, "my/indexLabel");
    
    // 状态刷新间隔
    static constexpr brls::Time TIME_UPDATE_INTERVAL_US = 1000000;      // 时间刷新间隔（1s）
    static constexpr brls::Time BATTERY_UPDATE_INTERVAL_US = 5000000;   // 电池百分比刷新间隔（5s）
    static constexpr brls::Time FPS_UPDATE_INTERVAL_US = 1000000;       // FPS 文本刷新间隔（1s）
    static constexpr brls::Time MEM_UPDATE_INTERVAL_US = 1000000;       // 内存采样间隔（1s）

    // 内容视图
    brls::View* m_contentView = nullptr;

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
    
    /**
     * @brief 设置内容视图
     * @param view 内容视图
     */
    void setContentView(brls::View* view);

    /** @brief 更新框架元素 */
    void updateFrameItem();

    /** @brief 更新状态栏文本 */
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
