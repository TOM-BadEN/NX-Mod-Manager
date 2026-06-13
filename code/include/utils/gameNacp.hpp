/**
 * gameNacp - 游戏 NACP 数据获取工具
 * 通过 appId 获取游戏名、版本号、图标数据
 * 回退链：libnxtc 缓存 → libnx ns 服务
 *
 * 使用前需调用 init()，程序退出前调用 cleanup()
 */

#pragma once

#include <string>
#include <vector>
#include <cstdint>

struct GameMetadata {
    std::string name;
    std::string version;
    std::vector<uint8_t> icon;  // JPEG 数据
};

namespace gameNacp {

/** @brief 初始化 ns/avm/nxtc 服务（程序启动时调用一次） */
void init();

/** @brief 刷新缓存并关闭服务（程序退出时调用一次） */
void cleanup();

/**
 * @brief 获取游戏 NACP 元数据
 * @param appId 游戏 appId
 */
GameMetadata getGameNACP(uint64_t appId);

/**
 * @brief 获取游戏的英文名（优先美式英文，其次英式英文）
 * @param appId 游戏 appId
 */
std::string getEnglishName(uint64_t appId);

/**
 * @brief 获取游戏版本号
 * @param appId 游戏 appId
 */
std::string getVersion(uint64_t appId);

/** @brief 枚举 Switch 上所有已安装的游戏 TID（过滤非游戏应用） */
std::vector<uint64_t> getInstalledGameTids();

}
