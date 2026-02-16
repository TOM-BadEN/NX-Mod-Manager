/**
 * ModScanner - Mod 扫描器
 * 扫描指定 TID 目录下的 mod 子目录 + 读取 mod.json 元数据
 */

#pragma once

#include <vector>
#include <string>
#include "common/modInfo.hpp"

class ModScanner {
public:
    // tidPath: 游戏的 TID 目录路径（GameInfo.dirPath）
    std::vector<ModInfo> scanMods(const std::string& tidPath);
};
