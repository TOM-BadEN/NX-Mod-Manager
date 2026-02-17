/**
 * ModList - Mod 列表页面
 * 左侧 RecyclingGrid（单列 Mod 列表） + 右侧 Mod 详情
 */

#include "activity/modList.hpp"
#include "scanner/modScanner.hpp"
#include "view/modCard.hpp"
#include "dataSource/modCardDS.hpp"
#include "utils/strSort.hpp"

ModList::ModList(const std::string& dirPath, const std::string& gameName)
    : m_dirPath(dirPath), m_gameName(gameName) {}

void ModList::onContentAvailable() {
    m_frame->setTitle(m_gameName);
    m_mods = ModScanner().scanMods(m_dirPath);

    setupModGrid();

    m_frame->registerAction("排序：升", brls::BUTTON_Y, [this](...) {
        toggleSort();
        return true;
    });
}

void ModList::setupModGrid() {
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

void ModList::toggleSort() {
    m_sortAsc = !m_sortAsc;
    strSort::sortAZ(m_mods, &ModInfo::displayName, &ModInfo::isInstalled, &ModInfo::type, m_sortAsc);
    m_grid->reloadData();
    auto* cell = m_grid->getGridItemByIndex(0);
    if (cell) brls::Application::giveFocus(cell);
    m_frame->updateActionHint(brls::BUTTON_Y, m_sortAsc ? "排序：升" : "排序：降");
    brls::Application::getGlobalHintsUpdateEvent()->fire();
}

void ModList::flipScreen(int direction) {
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