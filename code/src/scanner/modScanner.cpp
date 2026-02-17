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

    // ===== 测试用虚拟数据（上线前删除）=====
    mods.push_back({"60FPS 解锁补丁",     "performance",  "解锁游戏帧率至60FPS",         "1.0", "1.2.0", "FPSMaster",  "1.2 MB", true,  false, ""});
    mods.push_back({"4K 高清材质包",       "graphics",     "替换所有材质为4K分辨率",       "2.1", "1.2.0", "HDPack",     "256 MB", false, true,  ""});
    mods.push_back({"完整中文汉化",        "translation",  "全文本中文化，含剧情对话",     "3.0", "1.1.0", "CNTeam",     "18 MB",  true,  false, ""});
    mods.push_back({"全角色解锁",          "feature",      "开局解锁全部可玩角色",         "1.5", "1.2.0", "UnlockAll",  "0.5 MB", false, false, ""});
    mods.push_back({"极简 HUD 界面",       "ui",           "精简界面元素，沉浸体验",       "1.0", "1.0.0", "CleanUI",    "3.2 MB", true,  false, ""});
    mods.push_back({"原声音乐替换",        "music",        "替换BGM为管弦乐重编版",        "1.2", "1.2.0", "OST_Remix",  "89 MB",  false, true,  ""});
    mods.push_back({"暗黑骑士皮肤",        "skin",         "主角全套暗黑风格外观替换",     "2.0", "1.1.0", "SkinMod",    "45 MB",  true,  false, ""});
    mods.push_back({"无限金币金手指",      "cheat",        "金币数量锁定为999999",         "1.0", "1.2.0", "CheatKing",  "0.1 MB", false, false, ""});
    mods.push_back({"动态分辨率优化",      "performance",  "根据场景自动调节分辨率",       "1.3", "1.2.0", "DynRes",     "0.8 MB", false, false, ""});
    mods.push_back({"光影增强包",          "graphics",     "增强全局光照和阴影质量",       "1.5", "1.1.0", "LightFX",    "128 MB", true,  false, ""});
    mods.push_back({"日语语音包",          "translation",  "替换语音为日语原版配音",       "2.0", "1.2.0", "JPVoice",    "340 MB", false, false, ""});
    mods.push_back({"新游戏+模式",         "feature",      "通关后解锁二周目增强模式",     "1.0", "1.2.0", "NGPlus",     "2.1 MB", true,  false, ""});
    mods.push_back({"复古像素风 UI",       "ui",           "将界面替换为像素复古风格",     "1.1", "1.0.0", "PixelUI",    "5.6 MB", false, false, ""});
    mods.push_back({"战斗音乐增强",        "music",        "高燃战斗BGM替换合集",          "1.0", "1.1.0", "BattleBGM",  "67 MB",  true,  false, ""});
    mods.push_back({"夏日泳装皮肤",        "skin",         "全角色夏日泳装外观包",         "1.0", "1.2.0", "SummerSkin", "38 MB",  false, false, ""});
    mods.push_back({"经验值翻倍",          "cheat",        "战斗获取经验值翻倍",           "1.0", "1.2.0", "EXPx2",      "0.1 MB", true,  false, ""});
    mods.push_back({"帧率稳定补丁",        "performance",  "减少掉帧和卡顿现象",          "2.0", "1.1.0", "StableFPS",  "0.5 MB", true,  false, ""});
    mods.push_back({"HDR 色彩增强",        "graphics",     "启用HDR色彩映射增强画面",      "1.0", "1.2.0", "HDRColor",   "15 MB",  false, false, ""});
    mods.push_back({"繁体中文补丁",        "translation",  "简体转繁体全文本替换",         "1.2", "1.2.0", "TWChinese",  "8 MB",   true,  false, ""});
    mods.push_back({"快速旅行扩展",        "feature",      "增加更多快速旅行传送点",       "1.0", "1.1.0", "FastTravel",  "1.5 MB", false, false, ""});
    // ===== 测试用虚拟数据结束 =====

    // 三级排序：已安装 > 未安装 → 类型分组 → 拼音
    strSort::sortAZ(mods, &ModInfo::displayName, &ModInfo::isInstalled, &ModInfo::type);

    return mods;
}
