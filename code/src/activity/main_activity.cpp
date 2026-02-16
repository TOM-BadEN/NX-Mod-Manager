/**
 * 主页面实现文件
 */

#include "activity/main_activity.hpp"
#include "scanner/gameScanner.hpp"
#include "common/config.hpp"
#include "utils/gameNACP.hpp"
#include "utils/strSort.hpp"
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
    m_frame->registerAction("排序：升", brls::BUTTON_X, [this](...) {
        toggleSort();
        return true;
    });
    m_frame->setActionAvailable(brls::BUTTON_X, false);

    startNacpLoader();
}

void MainActivity::showEmptyHint() {
    m_gridPage->setVisibility(brls::Visibility::GONE);
    m_noModHint->setVisibility(brls::Visibility::VISIBLE);
    m_noModHint->setFocusable(true);
    m_noModHint->setHideHighlight(true);
    brls::Application::giveFocus(m_noModHint);
}

void MainActivity::setupGridPage() {
    m_gridPage->setGameList(m_games);
    m_gridPage->setIndexChangeCallback([this](int index, int total) {
        m_frame->setIndexText(std::to_string(index) + " / " + std::to_string(total));
        m_currentPage.store(m_gridPage->getCurrentPage());
    });
}

void MainActivity::toggleSort() {
    m_sortAsc = !m_sortAsc;
    strSort::sortAZ(m_games, &GameInfo::displayName, &GameInfo::isFavorite, m_sortAsc);
    m_gridPage->reloadData();
    m_frame->updateActionHint(brls::BUTTON_X, m_sortAsc ? "排序：升" : "排序：降");
    brls::Application::getGlobalHintsUpdateEvent()->fire();
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
            // 找离当前页最近的任务
            int page = m_currentPage.load();
            int center = page * 9 + 4;
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
            m_frame->setActionAvailable(brls::BUTTON_X, true);
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

    // 刷新卡片
    m_gridPage->updateCard(gameIdx);
}
