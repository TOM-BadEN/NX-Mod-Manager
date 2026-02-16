/**
 * ModInfo - Mod 信息结构体
 * 用于 ModList 显示、ModDetail 展示等模块共享
 */

#pragma once

#include <string>

// 服务器 type 字符串 → 中文显示
inline const char* modTypeText(const std::string& type) {
    if (type == "performance") return "性能优化";
    if (type == "graphics")    return "画质增强";
    if (type == "translation") return "游戏汉化";
    if (type == "feature")     return "功能扩展";
    if (type == "ui")          return "界面美化";
    if (type == "music")       return "音乐替换";
    if (type == "skin")        return "角色皮肤";
    if (type == "cheat")       return "金手指";
    return "其他";
}

struct ModInfo {
    std::string displayName;    // 显示名（回滚链：JSON displayName → 目录名）
    std::string type;           // 功能类型
    std::string description;    // mod 详情描述
    std::string modVersion;     // mod 版本
    std::string gameVersion;    // 适配的游戏版本
    std::string author;         // 作者
    std::string size;           // 体积（预格式化，如 "12.5 MB"）
    bool isInstalled = false;   // 是否已安装
    bool isZip = false;         // 是否为 ZIP 形式
    std::string path;           // 完整路径
};
