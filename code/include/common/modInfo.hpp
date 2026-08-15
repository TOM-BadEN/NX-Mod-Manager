/**
 * ModInfo - Mod 信息结构体
 * 用于 Mod 列表（RecyclingGrid）和详情面板等模块共享
 */

#pragma once

#include <string>
#include <vector>
#include <borealis/core/i18n.hpp>

/**
 * @brief 获取服务端 Mod 类型对应的本地化名称
 * @param type 服务端返回的 Mod 类型
 * @return 本地化类型名称
 */
inline std::string modTypeText(const std::string& type) {
    if (type == "performance") return brls::getStr("other/modType/performance");
    if (type == "graphics") return brls::getStr("other/modType/graphics");
    if (type == "translation") return brls::getStr("other/modType/translation");
    if (type == "feature") return brls::getStr("other/modType/feature");
    if (type == "ui") return brls::getStr("other/modType/ui");
    if (type == "music") return brls::getStr("other/modType/music");
    if (type == "skin") return brls::getStr("other/modType/skin");
    if (type == "cheat") return brls::getStr("other/modType/cheat");
    return brls::getStr("other/modType/other");
}

/**
 * @brief 获取服务端 Mod 类型对应的图标路径
 * @param type 服务端返回的 Mod 类型
 * @return 图标资源路径，无效类型使用 other 图标
 */
inline std::string modTypeIcon(const std::string& type) {
    static const std::string prefix = "img/mod/modType/";
    if (type == "performance" || type == "graphics" || type == "translation" ||
        type == "feature" || type == "ui" || type == "music" ||
        type == "skin" || type == "cheat" || type == "other")
        return prefix + type + ".jpg";
    return prefix + "other.jpg";
}

/** @brief Mod 类型选项 */
struct ModTypeOption {
    std::string value; // 服务端类型值
    std::string label; // 本地化类型名称
    std::string desc;  // 本地化类型说明
};

/** @brief 获取全部 Mod 类型选项 */
inline const std::vector<ModTypeOption>& modTypeOptions() {
    static const std::vector<ModTypeOption> options = {
        {"performance", brls::getStr("other/modType/performance"), brls::getStr("other/modTypeDesc/performance")},
        {"graphics", brls::getStr("other/modType/graphics"), brls::getStr("other/modTypeDesc/graphics")},
        {"translation", brls::getStr("other/modType/translation"), brls::getStr("other/modTypeDesc/translation")},
        {"feature", brls::getStr("other/modType/feature"), brls::getStr("other/modTypeDesc/feature")},
        {"ui", brls::getStr("other/modType/ui"), brls::getStr("other/modTypeDesc/ui")},
        {"music", brls::getStr("other/modType/music"), brls::getStr("other/modTypeDesc/music")},
        {"skin", brls::getStr("other/modType/skin"), brls::getStr("other/modTypeDesc/skin")},
        {"cheat", brls::getStr("other/modType/cheat"), brls::getStr("other/modTypeDesc/cheat")},
        {"other", brls::getStr("other/modType/other"), brls::getStr("other/modTypeDesc/other")},
    };
    return options;
}

/** @brief Mod 信息 */
struct ModInfo {
    std::string displayName;  // 显示名（回滚链：JSON displayName → 目录名）
    std::string type;         // 功能类型
    std::string description;  // Mod 详情描述
    std::string modVersion;   // Mod 版本
    std::string gameVersion;  // 适配的游戏版本
    std::string author;       // 作者
    std::string authorLink;   // 作者链接
    std::string size;         // 体积（预格式化，如 "12.5 MB"）
    bool isInstalled = false; // 是否已安装
    bool hasUpdate = false;   // 是否有可用更新（仅内存，不持久化）
    bool isZip = false;       // 是否为 ZIP 形式
    int modID = -1;           // 商店 Mod ID（无则 -1）
    std::string fileCrc32;    // Mod 文件 CRC32（8 位十六进制小写字符串，用于与服务端比对文件是否更新）
    std::string dirName;      // 目录名（JSON key）
    std::string path;         // Mod 目录路径
    bool isPending = false;         // 是否等待 FrameQueue 显示真实卡片
    bool isMetadataPending = false; // 本次页面停留期间是否等待补充体积或 CRC32
};
