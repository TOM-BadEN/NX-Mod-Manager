/**
 * config - 全局常量配置
 * 路径、文件名等硬编码常量，集中管理
 */

#pragma once

#include <vector>
#include <string>
#include <cstring>

namespace config {

    // ── MOD路径 ──

    constexpr const char* modsRoot     = "/mods2/";
    constexpr const char* gameInfoPath = "/mods2/gameInfo.json";
    constexpr const char* modInfoFile       = "/modInfo.json";
    constexpr const char* refCountFile      = "/modFileRefCount.json";
    constexpr const char* transitDir        = "/mods2/!temp_mods/";

    // ── 配置文件路径 ──

    constexpr const char* settingsPath       = "/config/NX-Mod-Manager/setting.json";
    constexpr const char* modShopDir          = "/config/NX-Mod-Manager/modShop/";
    constexpr const char* downloadRecordPath  = "/config/NX-Mod-Manager/modShop/downloadRecord.txt";
    constexpr const char* appUpdateDir        = "/config/NX-Mod-Manager/appUpdate/";

    // ── 网络 ──

    constexpr const char* websiteUrl = "https://tom-switch.fun";

    // ── 文件类型 ──

    inline const std::vector<std::string> modFileExts = {".zip"};

    // ── 运行时状态 ──

    inline char s_nroPath[512] = {0};
    inline void setNroPath(const char* path) { if (path) std::strncpy(s_nroPath, path, sizeof(s_nroPath) - 1); }
    inline const char* getNroPath() { return s_nroPath; }

} // namespace config
