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

    GameMetadata getGameNACP(uint64_t appId);
};
