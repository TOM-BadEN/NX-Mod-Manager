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
    
    void setTitle(std::string title);
    void setIcon(std::string path);
    void setIndexText(const std::string& text);
    
    static brls::View* create();
    
private:
    // XML 绑定的组件
    BRLS_BIND(brls::Box, m_header, "my/header");
    BRLS_BIND(brls::Box, m_contentBox, "my/content");
    BRLS_BIND(brls::Label, m_titleLabel, "my/title");
    BRLS_BIND(brls::Image, m_iconImage, "my/icon");
    BRLS_BIND(brls::Label, m_timeLabel, "my/time");
    BRLS_BIND(brls::Label, m_batteryPercentLabel, "my/battery_percent");
    BRLS_BIND(brls::Label, m_indexLabel, "my/indexLabel");
    
    // 成员变量
    brls::View* m_contentView = nullptr;
    std::string m_timeText;
    
    void setContentView(brls::View* view);
    void updateFrameItem();
    void updateStatusText();
};
