/**
 * ModList - Mod 列表页面
 * 左侧 RecyclingGrid（单列 Mod 列表） + 右侧 Mod 详情
 */

#include "activity/modList.hpp"
#include "scanner/modScanner.hpp"
#include "view/modCard.hpp"
#include "dataSource/modCardDS.hpp"
#include "utils/strSort.hpp"
#include "utils/format.hpp"
#include "utils/fsHelper.hpp"
#include "common/config.hpp"
#include <borealis/core/cache_helper.hpp>
#include <yoga/Yoga.h>
#include <switch.h>

ModList::ModList(const std::string& dirPath, const std::string& gameName, uint64_t appId)
    : m_dirPath(dirPath), m_gameName(gameName), m_appId(appId) {

}

void ModList::onContentAvailable() {
    m_frame->setTitle(m_gameName);
    m_modJson.load(m_dirPath + config::modInfoFile);
    m_mods = ModScanner().scanMods(m_dirPath, m_modJson);

    setupDetail();
    setupModGrid();

    if (!m_mods.empty()) updateDetail(0);

    m_frame->registerAction("排序：升", brls::BUTTON_Y, [this](...) {
        toggleSort();
        return true;
    });

    startSizeLoader();
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
        m_focusedIndex.store(index);
        m_frame->setIndexText(std::to_string(index + 1) + " / " + std::to_string(m_mods.size()));
        updateDetail(index);
    });

    m_grid->registerAction("上翻", brls::BUTTON_LB, [this](...) {
        flipScreen(-1);
        return true;
    }, true, true);
    m_grid->registerAction("下翻", brls::BUTTON_RB, [this](...) {
        flipScreen(1);
        return true;
    }, true, true);

    // 右键：列表 → 详情面板
    m_grid->registerAction("", brls::BUTTON_NAV_RIGHT, [this](...) {
        brls::Application::giveFocus(m_detail);
        return true;
    }, true, true);
}

void ModList::toggleSort() {
    m_sortAsc = !m_sortAsc;
    strSort::sortAZ(m_mods, &ModInfo::displayName, &ModInfo::isInstalled, &ModInfo::type, m_sortAsc);
    m_grid->reloadData();
    auto* cell = m_grid->getGridItemByIndex(0);
    if (cell) {
        auto style = brls::Application::getStyle();
        float saved = style["brls/animations/highlight"];
        style.addMetric("brls/animations/highlight", 1.0f);
        brls::Application::giveFocus(cell);
        style.addMetric("brls/animations/highlight", saved);
    }
    m_frame->updateActionHint(brls::BUTTON_Y, m_sortAsc ? "排序：升" : "排序：降");
    brls::Application::getGlobalHintsUpdateEvent()->fire();
}

void ModList::setupDetail() {
    // 标签区域启用自动换行（Borealis XML 不支持 flexWrap 属性，直接调 Yoga API）
    YGNodeStyleSetFlexWrap(m_tagRow->getYGNode(), YGWrapWrap);

    // 隐藏描述区滚动条（scrollingIndicatorVisible 非 XML 注册属性）
    m_scroll->setScrollingIndicatorVisible(false);

    // 左键：详情面板 → 列表（重置滚动 + 恢复焦点位置）
    m_detail->registerAction("", brls::BUTTON_NAV_LEFT, [this](...) {
        m_scroll->setContentOffsetY(0, false);
        auto* cell = m_grid->getGridItemByIndex(m_lastFocusIndex);
        if (cell) brls::Application::giveFocus(cell);
        else brls::Application::giveFocus(m_grid);
        return true;
    }, true, true);

    // 上下键：animated 驱动描述区滚动
    m_detail->registerAction("", brls::BUTTON_NAV_DOWN, [this](...) {
        float cur = m_scroll->getContentOffsetY();
        m_scroll->setContentOffsetY(cur + 60.0f, true);
        return true;
    }, true, true);
    m_detail->registerAction("", brls::BUTTON_NAV_UP, [this](...) {
        float cur = m_scroll->getContentOffsetY();
        m_scroll->setContentOffsetY(cur - 60.0f, true);
        return true;
    }, true, true);

    // 游戏名和 TID
    m_gameNameLabel->setText(m_gameName);
    std::string tid = format::appIdHex(m_appId);
    m_gameTid->setText(tid);

    // 从缓存取游戏图标（异步加载可能还没完成，启动重试）
    int iconId = brls::TextureCache::instance().getCache(tid);
    if (iconId > 0) m_gameIcon->innerSetImage(iconId);
    else retryIconLoad();
}

void ModList::updateDetail(size_t index) {
    if (index >= m_mods.size()) return;
    m_lastFocusIndex = index;
    m_scroll->setContentOffsetY(0, false);
    auto& mod = m_mods[index];
    m_tagType->setText(modTypeText(mod.type));
    m_tagVersion->setText(mod.modVersion.empty() ? "MOD 版本：未知" : "MOD 版本：" + mod.modVersion);
    m_tagAuthor->setText(mod.author.empty() ? "未知作者" : "By " + mod.author);
    if (mod.gameVersion.empty()) m_tagGameVer->setText("适配游戏：未知");
    else if (mod.gameVersion == "0") m_tagGameVer->setText("适配游戏：通用");
    else m_tagGameVer->setText("适配游戏：" + mod.gameVersion);
    m_tagSize->setText(mod.size.empty() ? "正在计算体积..." : mod.size);
    m_tagFormat->setText(mod.isZip ? "压缩包类型" : "文件类型");
    m_descBody->setText(mod.description.empty() ? "暂无描述" : mod.description);
}

void ModList::retryIconLoad() {
    if (m_iconRetryLeft <= 0) return;
    m_iconRetryLeft--;
    std::string tid = format::appIdHex(m_appId);
    int iconId = brls::TextureCache::instance().getCache(tid);
    if (iconId > 0) {
        m_gameIcon->innerSetImage(iconId);
        return;
    }
    brls::delay(1000, [this]() { retryIconLoad(); });
}

void ModList::startSizeLoader() {
    m_sizeLoader = util::async([this](std::stop_token token) {
        std::vector<int> tasks;
        for (int i = 0; i < static_cast<int>(m_mods.size()); i++) {
            if (m_mods[i].size.empty()) tasks.push_back(i);
        }

        while (!tasks.empty() && !token.stop_requested()) {
            int center = m_focusedIndex.load();
            int bestIdx = 0;
            int bestDist = INT_MAX;
            for (int i = 0; i < static_cast<int>(tasks.size()); i++) {
                int dist = std::abs(tasks[i] - center);
                if (dist < bestDist) {
                    bestDist = dist;
                    bestIdx = i;
                }
            }

            int modIdx = tasks[bestIdx];
            std::string modPath = m_mods[modIdx].path;
            tasks.erase(tasks.begin() + bestIdx);

            int64_t bytes = m_mods[modIdx].isZip ? fs::getFileSize(modPath) : fs::calcDirSize(modPath, &token);
            if (token.stop_requested()) break;
            if (bytes < 0) continue;
            std::string sizeStr = format::fileSize(bytes);

            // 回主线程更新数据和 UI
            brls::sync([this, modIdx, sizeStr = std::move(sizeStr)]() {
                applySizeResult(modIdx, sizeStr);
            });

            svcSleepThread(1000000ULL);  // 1ms
        }

        if (!token.stop_requested()) {
            brls::sync([this]() {
                m_modJson.save();
            });
        }
    });
}

void ModList::applySizeResult(int modIdx, const std::string& sizeStr) {
    m_mods[modIdx].size = sizeStr;
    if (static_cast<size_t>(modIdx) == m_lastFocusIndex) m_tagSize->setText(sizeStr);
    m_modJson.setString(m_mods[modIdx].dirName, "size", sizeStr);
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