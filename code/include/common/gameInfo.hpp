/**
 * GameInfo - 游戏信息结构体
 * 用于扫描阶段填充、RecyclingGrid 显示、JSON 读写等多个模块共享
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
    bool isDuplicate = false;    // 是否与其他条目共享同一个 appId
    bool isModsDisabled = false; // 是否已禁用所有模组
    bool hasInstalledMod = false; // 是否有已安装的模组
};

struct InstalledGameInfo {
    uint64_t appId = 0;       // 游戏唯一 ID
    std::string displayName;  // 显示名
    std::string version;      // 版本号
    std::string modCount;     // 已添加→实际数量，未添加→"未添加"
    int iconId = 0;           // NVG 图标 ID
    bool isLoaded = false;    // 数据是否完成加载
};
