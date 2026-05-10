/**
 * ModInfo - Mod 信息结构体
 * 用于 Mod 列表（RecyclingGrid）和详情面板等模块共享
 */

#pragma once

#include <string>
#include <vector>
#include <borealis/core/i18n.hpp>

// 服务器 type 字符串 → 本地化显示
inline std::string modTypeText(const std::string& type) {
    if (type == "performance") return brls::getStr("other/modType/performance");
    if (type == "graphics")    return brls::getStr("other/modType/graphics");
    if (type == "translation") return brls::getStr("other/modType/translation");
    if (type == "feature")     return brls::getStr("other/modType/feature");
    if (type == "ui")          return brls::getStr("other/modType/ui");
    if (type == "music")       return brls::getStr("other/modType/music");
    if (type == "skin")        return brls::getStr("other/modType/skin");
    if (type == "cheat")       return brls::getStr("other/modType/cheat");
    return brls::getStr("other/modType/other");
}

// 服务器 type 字符串 → 图标资源路径（无效 type 兜底到 "other"）
inline std::string modTypeIcon(const std::string& type) {
    static const std::string prefix = "img/mod/modType/";
    if (type == "performance" || type == "graphics" || type == "translation" ||
        type == "feature"     || type == "ui"       || type == "music" ||
        type == "skin"        || type == "cheat"    || type == "other")
        return prefix + type + ".jpg";
    return prefix + "other.jpg";
}

// Mod 类型选项（value / label / desc）
struct ModTypeOption {
    std::string value;
    std::string label;
    std::string desc;
};

inline const std::vector<ModTypeOption>& modTypeOptions() {
    static const std::vector<ModTypeOption> options = {
        {"performance", brls::getStr("other/modType/performance"), brls::getStr("other/modTypeDesc/performance")},
        {"graphics",    brls::getStr("other/modType/graphics"),    brls::getStr("other/modTypeDesc/graphics")},
        {"translation", brls::getStr("other/modType/translation"), brls::getStr("other/modTypeDesc/translation")},
        {"feature",     brls::getStr("other/modType/feature"),     brls::getStr("other/modTypeDesc/feature")},
        {"ui",          brls::getStr("other/modType/ui"),          brls::getStr("other/modTypeDesc/ui")},
        {"music",       brls::getStr("other/modType/music"),       brls::getStr("other/modTypeDesc/music")},
        {"skin",        brls::getStr("other/modType/skin"),        brls::getStr("other/modTypeDesc/skin")},
        {"cheat",       brls::getStr("other/modType/cheat"),       brls::getStr("other/modTypeDesc/cheat")},
        {"other",       brls::getStr("other/modType/other"),       brls::getStr("other/modTypeDesc/other")},
    };
    return options;
}

struct ModInfo {
    std::string displayName;    // 显示名（回滚链：JSON displayName → 目录名）
    std::string type;           // 功能类型
    std::string description;    // mod 详情描述
    std::string modVersion;     // mod 版本
    std::string gameVersion;    // 适配的游戏版本
    std::string author;         // 作者
    std::string authorLink;     // 作者链接
    std::string size;           // 体积（预格式化，如 "12.5 MB"）
    bool isInstalled = false;   // 是否已安装
    bool isZip = false;         // 是否为 ZIP 形式
    int modID = -1;             // 商店模组 ID（无则 -1）
    std::string dirName;        // 目录名（JSON key）
    std::string path;           // mod 目录路径
};
