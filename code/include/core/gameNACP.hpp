/**
 * GameNACP - 游戏数据获取封装
 * 通过 appId 获取游戏名、版本号、图标数据
 * 回退链：libnxtc 缓存 → libnx ns 服务
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

class GameNACP {
public:
    GameNACP();
    ~GameNACP();

    GameNACP(const GameNACP&) = delete;
    GameNACP& operator=(const GameNACP&) = delete;

    /**
     * @brief 获取游戏 NACP 元数据
     * @param appId 游戏 appId
     */
    GameMetadata getGameNACP(uint64_t appId);

    /**
     * @brief 获取游戏的英文名（优先美式英文，其次英式英文）
     * @param appId 游戏 appId
     */
    static std::string getEnglishName(uint64_t appId);

    /** @brief 枚举 Switch 上所有已安装的游戏 TID（过滤非游戏应用） */
    static std::vector<uint64_t> getInstalledGameTids();
};
