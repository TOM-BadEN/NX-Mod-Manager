/**
 * GameScanner - 主页游戏扫描
 * 第一阶段：快速扫描目录 + JSON，构建 GameInfo 列表
 * 第二阶段：异步调 API 更新 version + icon
 */

#pragma once

#include <vector>
#include <string>
#include "common/gameInfo.hpp"

class GameScanner {
public:
    std::vector<GameInfo> scanGames();

private:
    void sortGames(std::vector<GameInfo>& games);
};
