/**
 * gameNacp - 游戏 NACP 数据获取工具
 * 通过 appId 获取游戏名、版本号、图标数据
 * 回退链：libnxtc 缓存 → libnx ns 服务
 *
 * 使用前需调用 init()，程序退出前调用 cleanup()
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct GameMetadata {
    std::string name;         // 游戏名称
    std::string version;      // 游戏版本号
    std::vector<uint8_t> icon; // JPEG 图标数据
};

namespace gameNacp {

/** @brief 初始化 ns/avm/nxtc 服务（程序启动时调用一次） */
void init();

/** @brief 刷新缓存并关闭服务（程序退出时调用一次） */
void cleanup();

/**
 * @brief 获取游戏 NACP 元数据
 * @param appId 游戏 appId
 * @return 游戏名称、版本号和 JPEG 图标数据
 */
GameMetadata getGameNACP(uint64_t appId);

/**
 * @brief 获取游戏的英文名（优先美式英文，其次英式英文）
 * @param appId 游戏 appId
 * @return 游戏英文名，获取失败时返回空字符串
 */
std::string getEnglishName(uint64_t appId);

/**
 * @brief 获取游戏版本号
 * @param appId 游戏 appId
 * @return 游戏版本号，获取失败时返回空字符串
 */
std::string getVersion(uint64_t appId);

/**
 * @brief 枚举 Switch 上所有已安装的游戏 TID（过滤非游戏应用）
 * @return 已安装游戏的 TID 列表
 */
std::vector<uint64_t> getInstalledGameTids();

} // namespace gameNacp
