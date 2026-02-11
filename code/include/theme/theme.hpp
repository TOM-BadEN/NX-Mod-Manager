/**
 * 主题初始化
 * 从 resources/xml/theme/theme.xml 解析颜色并注册到框架
 * 支持 dark / light 双主题自适应
 */

#pragma once

#include <borealis.hpp>
#include <tinyxml2.h>

// 解析十六进制颜色字符串 (#RRGGBB 或 #RRGGBBAA) 为 NVGcolor
inline NVGcolor parseHexColor(const char* hex) {
    if (!hex || hex[0] != '#') return nvgRGB(255, 0, 255);
    
    size_t len = strlen(hex);
    int r, g, b, a = 255;
    
    if (len == 7) sscanf(hex + 1, "%02x%02x%02x", &r, &g, &b);
    else if (len == 9) sscanf(hex + 1, "%02x%02x%02x%02x", &r, &g, &b, &a);
    else return nvgRGB(255, 0, 255);
    
    return nvgRGBA(r, g, b, a);
}

// 从 XML 文件加载颜色并注册到 dark / light 双主题
inline void initTheme() {
    std::string path = std::string(BRLS_RESOURCES) + "xml/theme/theme.xml";
    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(path.c_str()) != tinyxml2::XML_SUCCESS) {
        brls::Logger::error("无法加载主题配置文件: {}", path);
        return;
    }
    
    auto* root = doc.FirstChildElement("theme");
    if (!root) return;
    
    auto& darkTheme = brls::Theme::getDarkTheme();
    auto& lightTheme = brls::Theme::getLightTheme();
    
    for (auto* elem = root->FirstChildElement("color"); elem; elem = elem->NextSiblingElement("color")) {
        const char* name = elem->Attribute("name");
        const char* dark = elem->Attribute("dark");
        const char* light = elem->Attribute("light");
        
        if (!name) continue;
        
        if (dark) darkTheme.addColor(name, parseHexColor(dark));
        if (light) lightTheme.addColor(name, parseHexColor(light));
    }
}
