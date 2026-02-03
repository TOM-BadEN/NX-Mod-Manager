/**
 * CustomAppletFrame - 自定义 AppletFrame
 * 
 * 继承自 brls::Box，完全自定义布局：
 * - Header：标题 + 图标（左）、时间 + WiFi + 电量% + 电池（右）
 * - Content：内容区域
 * - Footer：按钮提示
 */

#pragma once

#include <borealis.hpp>

class CustomAppletFrame : public brls::Box
{
public:
    CustomAppletFrame();
    
    void handleXMLElement(tinyxml2::XMLElement* element) override;
    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) override;
    
    void setTitle(std::string title);
    void setIcon(std::string path);
    
    static brls::View* create();
    
private:
    BRLS_BIND(brls::Box, header, "custom/header");
    BRLS_BIND(brls::Box, contentBox, "custom/content");
    BRLS_BIND(brls::Label, titleLabel, "custom/title");
    BRLS_BIND(brls::Image, iconImage, "custom/icon");
    BRLS_BIND(brls::Label, timeLabel, "custom/time");
    BRLS_BIND(brls::Label, batteryPercentLabel, "custom/battery_percent");
    
    brls::View* contentView = nullptr;
    std::string timeText;
    
    void setContentView(brls::View* view);
    void updateAppletFrameItem();
    void updateStatusText();
};
