/**
 * Theme - 应用自定义主题颜色
 *
 * 从 resources/xml/theme/theme.xml 解析颜色，并注册到 Borealis 的
 * 深色和浅色主题中。
 */

#pragma once

#include <borealis.hpp>
#include <tinyxml2.h>
#include <cstdio>
#include <cstring>
#include <string>

/**
 * @brief 解析十六进制颜色字符串 (#RRGGBB 或 #RRGGBBAA) 为 NVGcolor
 * @param hex 颜色字符串
 * @return 解析后的颜色，格式无效时返回洋红色
 */
inline NVGcolor parseHexColor(const char* hex) {
    if (!hex || hex[0] != '#') return nvgRGB(255, 0, 255);
    
    size_t len = strlen(hex);
    int r, g, b, a = 255;
    
    if (len == 7) sscanf(hex + 1, "%02x%02x%02x", &r, &g, &b);
    else if (len == 9) sscanf(hex + 1, "%02x%02x%02x%02x", &r, &g, &b, &a);
    else return nvgRGB(255, 0, 255);
    
    return nvgRGBA(r, g, b, a);
}

/** @brief 从 XML 文件加载颜色并注册到 dark / light 双主题 */
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
