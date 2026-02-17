/**
 * 主页面实现文件
 */

#include "activity/main_activity.hpp"
#include "scanner/gameScanner.hpp"
#include "common/config.hpp"
#include "utils/gameNACP.hpp"
#include "utils/strSort.hpp"
#include "activity/modManager.hpp"
#include "view/gameCard.hpp"
#include "dataSource/gameCardDS.hpp"
#include <borealis/core/cache_helper.hpp>
#include <switch.h>

void MainActivity::onContentAvailable() {
    m_jsonCache.load(config::gameInfoPath);
    m_games = GameScanner().scanGames(m_jsonCache);

    // 如果为空提示找不到mod
    if (m_games.empty()) {
        showEmptyHint();
        return;
    }

    setupGridPage();

    // X 键：切换排序方向（页面级操作，NACP 加载完成前禁用）
    m_frame->registerAction("排序：升", brls::BUTTON_Y, [this](...) {
        toggleSort();
        return true;
    });
    m_frame->setActionAvailable(brls::BUTTON_Y, false);

    startNacpLoader();
}

void MainActivity::showEmptyHint() {
    m_grid->setVisibility(brls::Visibility::GONE);
    m_noModHint->setVisibility(brls::Visibility::VISIBLE);
    m_noModHint->setFocusable(true);
    m_noModHint->setHideHighlight(true);
    brls::Application::giveFocus(m_noModHint);
}

void MainActivity::setupGridPage() {
    m_grid->setPadding(20, 40, 20, 40);
    m_grid->registerCell("GameCard", GameCard::create);

    auto* ds = new GameCardDS(m_games, [this](size_t index) {
        auto& game = m_games[index];
        brls::Application::pushActivity(new ModManager(game.dirPath, game.displayName));
    });
    m_grid->setDataSource(ds);

    m_grid->setFocusChangeCallback([this](size_t index) {
        m_focusedIndex.store(index);
        m_frame->setIndexText(std::to_string(index + 1) + " / " + std::to_string(m_games.size()));
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

void MainActivity::toggleSort() {
    m_sortAsc = !m_sortAsc;
    strSort::sortAZ(m_games, &GameInfo::displayName, &GameInfo::isFavorite, m_sortAsc);
    m_grid->reloadData();
    m_frame->updateActionHint(brls::BUTTON_Y, m_sortAsc ? "排序：升" : "排序：降");
    brls::Application::getGlobalHintsUpdateEvent()->fire();
}

void MainActivity::flipScreen(int direction) {
    auto* focus = brls::Application::getCurrentFocus();
    if (!focus) return;
    while (focus && !dynamic_cast<RecyclingGridItem*>(focus))
        focus = focus->getParent();
    if (!focus) return;
    size_t idx = static_cast<RecyclingGridItem*>(focus)->getIndex();

    int rowsPerScreen = std::max(1, static_cast<int>(m_grid->getHeight() / m_grid->estimatedRowHeight));
    int target = static_cast<int>(idx) + direction * m_grid->spanCount * rowsPerScreen;
    target = std::clamp(target, 0, static_cast<int>(m_grid->getDataSource()->getItemCount()) - 1);
    if (static_cast<size_t>(target) == idx) return;

    // selectRowAt 确保 target Cell 在 contentBox 中
    m_grid->selectRowAt(target, false);
    auto* cell = m_grid->getGridItemByIndex(target);
    if (!cell) return;

    // 1ms 动画 trick 防高亮闪烁
    auto style = brls::Application::getStyle();
    float saved = style["brls/animations/highlight"];
    style.addMetric("brls/animations/highlight", 1.0f);
    brls::Application::giveFocus(cell);
    style.addMetric("brls/animations/highlight", saved);
}

void MainActivity::startNacpLoader() {
    m_nacpLoader = util::async([this](std::stop_token token) {
        // 构建任务列表（存索引，appId 通过 m_games 取）
        std::vector<int> tasks;
        for (int i = 0; i < static_cast<int>(m_games.size()); i++) {
            tasks.push_back(i);
        }

        GameNACP nacp;

        while (!tasks.empty() && !token.stop_requested()) {
            // 找离当前焦点最近的任务
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

            int gameIdx = tasks[bestIdx];
            uint64_t appId = m_games[gameIdx].appId;
            tasks.erase(tasks.begin() + bestIdx);

            // 调 API 拿数据（后台线程）
            auto meta = nacp.getGameNACP(appId);
            if (meta.name.empty() && meta.version.empty() && meta.icon.empty()) continue;

            // 回主线程更新数据和 UI
            brls::sync([this, gameIdx, meta = std::move(meta)]() {
                applyMetadata(gameIdx, meta);
            });

            svcSleepThread(1000000ULL);  // 1ms
        }

        // 全部加载完后统一写回 JSON，并启用排序按钮
        brls::sync([this]() {
            m_jsonCache.save();
            m_frame->setActionAvailable(brls::BUTTON_Y, true);
        });
    });
}

void MainActivity::applyMetadata(int gameIdx, const GameMetadata& meta) {
    char appIdKey[17];
    std::snprintf(appIdKey, sizeof(appIdKey), "%016lX", m_games[gameIdx].appId);

    // 更新 version
    if (!meta.version.empty()) {
        m_games[gameIdx].version = meta.version;
        m_jsonCache.setString(appIdKey, "version", meta.version);
    }

    // 更新 displayName（用户自定义优先）
    if (!meta.name.empty()) {
        m_jsonCache.setString(appIdKey, "gameName", meta.name);
        if (m_jsonCache.getString(appIdKey, "displayName").empty()) {
            m_games[gameIdx].displayName = meta.name;
        }
    }

    // 创建 NVG 纹理（主线程安全），缓存交给框架管理
    if (!meta.icon.empty()) {
        NVGcontext* vg = brls::Application::getNVGContext();
        int iconId = nvgCreateImageMem(vg, 0, const_cast<unsigned char*>(meta.icon.data()), meta.icon.size());
        if (iconId > 0) {
            m_games[gameIdx].iconId = iconId;
            brls::TextureCache::instance().addCache(appIdKey, iconId);
        }
    }

    // 刷新可见 Cell（不可见的下次 cellForRow 自然绑定）
    auto* cell = m_grid->getGridItemByIndex(gameIdx);
    if (cell) {
        auto* card = static_cast<GameCard*>(cell);
        card->setGame(m_games[gameIdx].displayName, m_games[gameIdx].version, m_games[gameIdx].modCount);
        if (m_games[gameIdx].iconId > 0) card->setIcon(m_games[gameIdx].iconId);
    }
}
