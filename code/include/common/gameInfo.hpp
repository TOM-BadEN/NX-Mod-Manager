/**
 * GameInfo - 游戏信息结构体
 * 用于扫描阶段填充、GridPage 显示、JSON 读写等多个模块共享
 */

#pragma once

#include <string>
#include <cstdint>

struct GameInfo {
    std::string displayName;  // 显示名（回滚链：JSON displayName → JSON gameName → 目录名）
    std::string version;      // 版本号（从 JSON 缓存读，第二阶段 API 更新）
    std::string modCount;     // mod 数量
    int iconId = 0;           // NVG 图标 ID
    uint64_t appId = 0;       // 游戏唯一 ID
    std::string dirPath;      // 完整路径 /mods2/dirName/appIdHex
    bool isFavorite = false;  // 是否收藏
};
