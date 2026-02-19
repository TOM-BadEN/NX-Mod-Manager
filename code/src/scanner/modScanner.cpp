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
        info.modVersion  = json.getString(dirName, "modVersion");
        info.author      = json.getString(dirName, "author");
        info.size        = json.getString(dirName, "size");
        info.isInstalled = json.getBool(dirName, "installed", false);
        info.isZip       = false;
        info.path        = tidPath + "/" + dirName;

        info.gameVersion = json.getString(dirName, "gameVersion");

        mods.push_back(std::move(info));
    }

    // ===== 测试用虚拟数据（上线前删除）=====
    mods.push_back({"光影增强包",          "graphics",     "全面增强游戏的全局光照和阴影渲染质量，支持动态光源和环境光遮蔽效果",       "1.5", "1.1.0", "LightFX",    "128 MB", true,  false, "/mods2/Chained Echoes[1.31]/0100C510166F0000/testMod"});
    mods.push_back({"日语语音包",          "translation",  "将全部角色语音替换为日语原版配音，包含战斗语音、剧情对话和系统提示音",       "2.0", "1.2.0", "JPVoice",    "340 MB", false, false, "/mods2/Chained Echoes[1.31]/0100C510166F0000/testMod"});
    mods.push_back({"新游戏+模式",         "feature",      "通关后解锁二周目增强模式，保留全部装备和技能，敌人强度提升至两倍",     "1.0", "1.2.0", "NGPlus",     "2.1 MB", true,  false, "/mods2/Chained Echoes[1.31]/0100C510166F0000/testMod"});
    mods.push_back({"复古像素风 UI",       "ui",           "将全部菜单界面和HUD替换为复古像素艺术风格，包含自定义字体和图标",     "1.1", "1.0.0", "PixelUI",    "5.6 MB", false, false, "/mods2/Chained Echoes[1.31]/0100C510166F0000/testMod"});
    mods.push_back({"战斗音乐增强",        "music",        "替换全部战斗场景的背景音乐为高燃原创配乐，支持Boss战和普通战斗分别配置",          "1.0", "1.1.0", "BattleBGM",  "67 MB",  true,  false, "/mods2/Chained Echoes[1.31]/0100C510166F0000/testMod"});
    mods.push_back({"夏日泳装皮肤",        "skin",         "包含全部可玩角色的夏日泳装外观替换，附带新的待机动作和特效",         "1.0", "1.2.0", "SummerSkin", "38 MB",  false, false, "/mods2/Chained Echoes[1.31]/0100C510166F0000/testMod"});
    mods.push_back({"经验值翻倍",          "cheat",        "战斗结束后获取的经验值和金币数量翻倍，适合快速练级和后期刷战斗结束后获取的经验值和金币数量翻倍，适合快速练级和后期刷装备战斗结束后获取的经验值和金币数量翻倍，适合快速练级和后期刷装备战斗结束后获取的经验值和金币数量翻倍，适合快速练级和后期刷装备战斗结束后获取的经验值和金币数量翻倍，适合快速练级和后期刷装备战斗结束后获取的经验值和金币数量翻倍，适合快速练级和后期刷装备战斗结束后获取的经验值和金币数量翻倍，适合快速练级和后期刷装备战斗结束后获取的经验值和金币数量翻倍，适合快速练级和后期刷装备战斗结束后获取的经验值和金币数量翻倍，适合快速练级和后期刷装备战斗结束后获取的经验值和金币数量翻倍，适合快速练级和后期刷装备战斗结束后获取的经验值和金币数量翻倍，适合快速练级和后期刷装备战斗结束后获取的经验值和金币数量翻倍，适合快速练级和后期刷装备战斗结束后获取的经验值和金币数量翻倍，适合快速练级和后期刷装备装备",           "1.0", "1.2.0", "EXPx2",      "0.1 MB", true,  false, "/mods2/Chained Echoes[1.31]/0100C510166F0000/testMod"});
    mods.push_back({"帧率稳定补丁",        "performance",  "优化游戏引擎的渲染管线，大幅减少场景切换和战斗中的掉帧卡顿现象",          "2.0", "1.1.0", "StableFPS",  "0.5 MB", true,  false, "/mods2/Chained Echoes[1.31]/0100C510166F0000/testMod"});
    mods.push_back({"HDR 色彩增强",        "graphics",     "启用HDR色彩映射技术增强画面表现力，提升暗部细节和高光对比度",      "1.0", "1.2.0", "HDRColor",   "15 MB",  false, false, "/mods2/Chained Echoes[1.31]/0100C510166F0000/testMod"});
    mods.push_back({"繁体中文补丁",        "translation",  "将游戏内全部简体中文文本转换为繁体中文，包含菜单、对话和系统提示",         "1.2", "1.2.0", "TWChinese",  "8 MB",   true,  false, "/mods2/Chained Echoes[1.31]/0100C510166F0000/testMod"});
    mods.push_back({"快速旅行扩展",        "feature",      "在地图各区域增加更多快速旅行传送点，减少跑图时间提升探索效率",       "1.0", "1.1.0", "FastTravel",  "1.5 MB", false, false, "/mods2/Chained Echoes[1.31]/0100C510166F0000/testMod"});
    // ===== 测试用虚拟数据结束 =====

    // 三级排序：已安装 > 未安装 → 类型分组 → 拼音
    strSort::sortAZ(mods, &ModInfo::displayName, &ModInfo::isInstalled, &ModInfo::type);

    return mods;
}
