/**
 * 颜色注册
 * 从 resources/xml/theme/colors.xml 解析颜色并注册到框架
 */

#pragma once

#include <borealis.hpp>
#include <tinyxml2.h>

// 解析十六进制颜色字符串 (#RRGGBB) 为 NVGcolor
inline NVGcolor parseHexColor(const char* hex)
{
    if (!hex || hex[0] != '#' || strlen(hex) != 7)
        return nvgRGB(255, 0, 255); // 错误时返回品红色，便于发现问题
    
    int r, g, b;
    sscanf(hex + 1, "%02x%02x%02x", &r, &g, &b);
    return nvgRGB(r, g, b);
}

// 从 XML 文件加载颜色并注册
inline void initColor()
{
    // 强制使用暗色主题
    brls::Application::getPlatform()->setThemeVariant(brls::ThemeVariant::DARK);
    
    // 解析颜色 XML
    std::string path = std::string(BRLS_RESOURCES) + "xml/theme/colors.xml";
    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(path.c_str()) != tinyxml2::XML_SUCCESS)
    {
        brls::Logger::error("无法加载颜色配置文件: {}", path);
        return;
    }
    
    auto* root = doc.FirstChildElement("theme");
    if (!root) return;
    
    auto& theme = brls::Theme::getDarkTheme();
    
    // 遍历所有 <color> 节点
    for (auto* elem = root->FirstChildElement("color"); elem; elem = elem->NextSiblingElement("color"))
    {
        const char* name = elem->Attribute("name");
        const char* value = elem->GetText();
        
        if (name && value)
        {
            theme.addColor(name, parseHexColor(value));
        }
    }
}
