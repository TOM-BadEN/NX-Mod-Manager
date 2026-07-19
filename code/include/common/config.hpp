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
    constexpr const char* mhriseInstallMapFile = "/mhriseInstallMap.json";
    constexpr const char* dontStarveInstallMapFile = "/dontStarveInstallMap.json";
    constexpr const char* transitDir        = "/mods2/!temp_mods/";

    // ── 配置文件路径 ──

    constexpr const char* settingsPath       = "/config/NX-Mod-Manager/setting.json";
    constexpr const char* modShopDir          = "/config/NX-Mod-Manager/modShop/";
    constexpr const char* storeGameIconDir    = "/config/NX-Mod-Manager/modShop/gameIcons";
    constexpr const char* storeGameIconCachePath = "/config/NX-Mod-Manager/modShop/gameIconCache.json";
    constexpr const char* appUpdateDir        = "/config/NX-Mod-Manager/appUpdate/";

    // ── 网络 ──

    constexpr const char* websiteUrl = "https://tom-switch.fun";
    constexpr const char* donateWechatQr = "wxp://f2f0mMaZS-xnKyAZaTyn813TQRZHTRKPzI6UWxldiWDYemRIq8Stt_LPfd1sLyV4v1jR";
    constexpr const char* donatePaypalQr = "https://www.paypal.com/qrcodes/p2pqrc/7CQ7FTPN26AJ8";
    constexpr const char* communityQqQr = "https://qm.qq.com/cgi-bin/qm/qr?k=PrrCMC64n5PkJVchBhIxPWeRPq4UT3Rx&jump_from=webapi&authKey=wnTddnj5O40A2ktYbCPuiyuJt0715BsKcfGYbr/B0IFGrAqbwToVgA4GLY+ucd6g";
    constexpr const char* communityDiscordQr = "https://discord.gg/hPymY4XpAX";
    constexpr const char* projectGithubUrl = "https://github.com/TOM-BadEN/NX-Mod-Manager";
    constexpr const char* guideBilibiliUrl = "https://www.bilibili.com/video/BV1zd5o6wE5B/";

    // ── 文件类型 ──

    inline const std::vector<std::string> modFileExts = {".zip"};

    // ── 运行时状态 ──

    inline char s_nroPath[512] = {0};
    inline void setNroPath(const char* path) { if (path) std::strncpy(s_nroPath, path, sizeof(s_nroPath) - 1); }
    inline const char* getNroPath() { return s_nroPath; }

} // namespace config
