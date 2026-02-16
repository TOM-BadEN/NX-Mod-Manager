/**
 * config - 全局常量配置
 * 路径、文件名等硬编码常量，集中管理
 */

#pragma once

#include <vector>
#include <string>

namespace config {

    constexpr const char* modsRoot     = "/mods2/";
    constexpr const char* gameInfoPath = "/mods2/gameInfo.json";
    constexpr const char* modInfoFile  = "/modInfo.json";

    inline const std::vector<std::string> modFileExts = {".zip"};

} // namespace config
