/**
 * ModScanner - Mod 扫描器
 * 扫描指定 TID 目录下的 mod 子目录 + 读取 mod.json 元数据
 */

#include "scanner/modScanner.hpp"
#include "utils/fsHelper.hpp"
#include "utils/jsonFile.hpp"
#include "utils/strSort.hpp"
#include "common/config.hpp"

std::vector<ModInfo> ModScanner::scanMods(const std::string& tidPath) {
    std::vector<ModInfo> mods;

    // 加载 modInfo.json
    JsonFile json;
    json.load(tidPath + config::modInfoFile);

    // 列出所有 mod 子目录
    auto dirs = fs::listSubDirs(tidPath);

    for (const auto& dirName : dirs) {
        ModInfo info;

        // 显示名回滚：displayName → 目录名
        std::string displayName = json.getString(dirName, "displayName");
        info.displayName = displayName.empty() ? dirName : displayName;

        // 元数据
        info.type        = json.getString(dirName, "type", "other");
        info.description = json.getString(dirName, "description");
        info.modVersion  = json.getString(dirName, "modVersion", "版本未知");
        info.author      = json.getString(dirName, "author");
        info.size        = json.getString(dirName, "size", "...");
        info.isInstalled = json.getBool(dirName, "installed", false);
        info.isZip       = false;
        info.path        = tidPath + "/" + dirName;

        // gameVersion 有特殊值处理
        std::string gameVer = json.getString(dirName, "gameVersion","适配版本未知");
        if (gameVer == "0") info.gameVersion = "游戏版本通用";
        else info.gameVersion = gameVer;

        mods.push_back(std::move(info));
    }

    // 按显示名拼音排序
    strSort::sortAZ(mods, &ModInfo::displayName);

    return mods;
}
