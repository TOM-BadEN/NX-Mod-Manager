/**
 * ModManager - Mod 管理页面
 * 左侧 RecyclingGrid（单列 Mod 列表） + 右侧 Mod 详情
 */

#include "activity/modManager.hpp"
#include "scanner/modScanner.hpp"
#include "view/modCard.hpp"
#include "dataSource/modCardDS.hpp"

ModManager::ModManager(const std::string& dirPath, const std::string& gameName)
    : m_dirPath(dirPath), m_gameName(gameName) {}

void ModManager::onContentAvailable() {
    m_frame->setTitle(m_gameName);
    m_mods = ModScanner().scanMods(m_dirPath);

    // ===== 测试用虚拟数据（上线前删除）=====
    m_mods.push_back({"60FPS 解锁补丁",     "performance",  "解锁游戏帧率至60FPS",         "1.0", "1.2.0", "FPSMaster",  "1.2 MB", true,  false, ""});
    m_mods.push_back({"4K 高清材质包",       "graphics",     "替换所有材质为4K分辨率",       "2.1", "1.2.0", "HDPack",     "256 MB", false, true,  ""});
    m_mods.push_back({"完整中文汉化",        "translation",  "全文本中文化，含剧情对话",     "3.0", "1.1.0", "CNTeam",     "18 MB",  true,  false, ""});
    m_mods.push_back({"全角色解锁",          "feature",      "开局解锁全部可玩角色",         "1.5", "1.2.0", "UnlockAll",  "0.5 MB", false, false, ""});
    m_mods.push_back({"极简 HUD 界面",       "ui",           "精简界面元素，沉浸体验",       "1.0", "1.0.0", "CleanUI",    "3.2 MB", true,  false, ""});
    m_mods.push_back({"原声音乐替换",        "music",        "替换BGM为管弦乐重编版",        "1.2", "1.2.0", "OST_Remix",  "89 MB",  false, true,  ""});
    m_mods.push_back({"暗黑骑士皮肤",        "skin",         "主角全套暗黑风格外观替换",     "2.0", "1.1.0", "SkinMod",    "45 MB",  true,  false, ""});
    m_mods.push_back({"无限金币金手指",      "cheat",        "金币数量锁定为999999",         "1.0", "1.2.0", "CheatKing",  "0.1 MB", false, false, ""});
    // ===== 测试用虚拟数据结束 =====

    setupModGrid();
}

void ModManager::setupModGrid() {
    m_grid->setPadding(25, 0, 20, 40);
    m_grid->setScrollingIndicatorVisible(false);
    m_grid->registerCell("ModCard", ModCard::create);

    auto* ds = new ModCardDS(m_mods, [this](size_t index) {
        // TODO: 点击 mod 的操作（安装/卸载等）
    });
    m_grid->setDataSource(ds);

    m_grid->setFocusChangeCallback([this](size_t index) {
        m_frame->setIndexText(std::to_string(index + 1) + " / " + std::to_string(m_mods.size()));
    });

    m_grid->registerAction("上翻", brls::BUTTON_LB, [this](...) {
        flipScreen(-1);
        return true;
    }, true, true);
    m_grid->registerAction("下翻", brls::BUTTON_RB, [this](...) {
        flipScreen(1);
        return true;
    }, true, true);
}

void ModManager::flipScreen(int direction) {
    auto* focus = brls::Application::getCurrentFocus();
    if (!focus) return;
    while (focus && !dynamic_cast<RecyclingGridItem*>(focus))
        focus = focus->getParent();
    if (!focus) return;
    size_t idx = static_cast<RecyclingGridItem*>(focus)->getIndex();

    int rowsPerScreen = std::max(1, static_cast<int>(m_grid->getHeight() / (m_grid->estimatedRowHeight + m_grid->estimatedRowSpace)));
    int target = static_cast<int>(idx) + direction * m_grid->spanCount * rowsPerScreen;
    target = std::clamp(target, 0, static_cast<int>(m_grid->getDataSource()->getItemCount()) - 1);

    if (static_cast<size_t>(target) == idx) {
        auto* cur = brls::Application::getCurrentFocus();
        if (cur) cur->shakeHighlight(direction > 0 ? brls::FocusDirection::DOWN : brls::FocusDirection::UP);
        return;
    }

    m_grid->selectRowAt(target, false);
    auto* cell = m_grid->getGridItemByIndex(target);
    if (!cell) return;

    auto style = brls::Application::getStyle();
    float saved = style["brls/animations/highlight"];
    style.addMetric("brls/animations/highlight", 1.0f);
    brls::Application::giveFocus(cell);
    style.addMetric("brls/animations/highlight", saved);
}