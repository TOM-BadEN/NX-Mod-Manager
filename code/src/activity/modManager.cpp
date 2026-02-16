/**
 * ModManager - Mod 管理页面
 * 左侧 ModList + 右侧 ModDetail，扫描并显示某个游戏的所有 mod
 */

#include "activity/modManager.hpp"
#include "scanner/modScanner.hpp"

ModManager::ModManager(const std::string& dirPath, const std::string& gameName)
    : m_dirPath(dirPath), m_gameName(gameName) {}

void ModManager::onContentAvailable() {
    // 设置标题
    m_frame->setTitle(m_gameName);

    // 扫描 mod
    m_mods = ModScanner().scanMods(m_dirPath);

    // 设置列表数据
    m_modList->setModList(m_mods);

    // 索引更新回调
    m_modList->setIndexChangeCallback([this](int index, int total) {
        m_frame->setIndexText(std::to_string(index) + " / " + std::to_string(total));
    });
}