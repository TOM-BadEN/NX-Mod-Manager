/**
 * modGameType - MOD 适配视角下的游戏类型识别
 */

#pragma once

#include <cstdint>

/**
 * @brief MOD 适配视角下的游戏类型
 */
enum class ModGameType {
    Normal,      // 普通游戏
    MHRise,      // 怪物猎人 崛起
    DontStarve,  // 饥荒
};

namespace modGameType {

/**
 * @brief 根据游戏 appId 获取 MOD 游戏类型
 * @param appId 游戏 appId
 */
ModGameType detect(uint64_t appId);

} // namespace modGameType
