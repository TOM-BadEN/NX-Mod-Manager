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
}