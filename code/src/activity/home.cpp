/**
 * 主页面实现文件
 */

#include "activity/home.hpp"
#include "scanner/gameScanner.hpp"
#include "common/config.hpp"
#include "utils/gameNACP.hpp"
#include "utils/strSort.hpp"
#include "activity/modList.hpp"
#include "view/gameCard.hpp"
#include "dataSource/gameCardDS.hpp"
#include "utils/format.hpp"
#include <borealis/core/cache_helper.hpp>
#include <switch.h>

void Home::onContentAvailable() {
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

    setupMenu();
    startNacpLoader();
}

void Home::showEmptyHint() {
    m_grid->setVisibility(brls::Visibility::GONE);
    m_noModHint->setVisibility(brls::Visibility::VISIBLE);
    m_noModHint->setFocusable(true);
    m_noModHint->setHideHighlight(true);
    brls::Application::giveFocus(m_noModHint);
}

void Home::setupGridPage() {
    m_grid->setPadding(17, 15, 17, 40);
    m_grid->registerCell("GameCard", GameCard::create);

    auto* ds = new GameCardDS(m_games, [this](size_t index) {
        auto& game = m_games[index];
        brls::Application::pushActivity(new ModList(game));
    });
    m_grid->setDataSource(ds);

    m_grid->setFocusChangeCallback([this](size_t index) {
        m_focusedIndex.store(index);
        m_frame->setIndexText(std::to_string(index + 1) + " / " + std::to_string(m_games.size()));
    });
}

void Home::toggleSort() {
    m_sortAsc = !m_sortAsc;
    strSort::sortAZ(m_games, &GameInfo::displayName, &GameInfo::isFavorite, m_sortAsc);
    m_grid->setDefaultCellFocus(0);  // 11.8: 排序后回到顶部
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

void Home::toggleFavorite() {
    int idx = m_focusedIndex.load();
    auto& game = m_games[idx];

    game.isFavorite = !game.isFavorite;

    std::string appIdKey = format::appIdHex(game.appId);
    uint64_t targetAppId = game.appId;
    m_jsonCache.setBool(appIdKey, "favorite", game.isFavorite);
    m_jsonCache.save();

    strSort::sortAZ(m_games, &GameInfo::displayName, &GameInfo::isFavorite, m_sortAsc);

    // 找到目标游戏排序后的新位置
    int newIdx = 0;
    for (int i = 0; i < (int)m_games.size(); i++) {
        if (m_games[i].appId == targetAppId) { newIdx = i; break; }
    }

    m_grid->setDefaultCellFocus(newIdx);
    m_grid->reloadData();
    auto* cell = m_grid->getGridItemByIndex(newIdx);
    if (cell) {
        auto style = brls::Application::getStyle();
        float saved = style["brls/animations/highlight"];
        style.addMetric("brls/animations/highlight", 1.0f);
        brls::Application::giveFocus(cell);
        style.addMetric("brls/animations/highlight", saved);
    }
}

void Home::setupMenu() {
    m_menu = {"菜单选项", {

        MenuItemConfig{"收藏游戏", "将当前游戏加入/取消收藏，收藏的游戏会优先显示在列表顶部"}
            .title([this]{ return m_games[m_focusedIndex.load()].isFavorite ? "取消收藏" : "收藏游戏"; })
            .action([this]{ toggleFavorite(); }),

        MenuItemConfig{"管理游戏", "包含以下内容：\n - 修改游戏的元数据\n - 新增、移除当前游戏 \n - 查看游戏的SD卡路径"},
        MenuItemConfig{"文件传输", "通过 MTP 传输 Mod 文件到 SD 卡上"},
        MenuItemConfig{"功能设置", "包含以下内容：\n - 待定"},

        MenuItemConfig{"检查更新", "用于查看和更新最新的插件版本"}
            .badge("暂无更新")
            .badgeHighlight([]{ return false; }),

        MenuItemConfig{"关于插件", "包含以下内容：\n - 使用说明\n - 基本信息\n - 反馈与建议渠道\n - 赞助支持"},

    }};

    m_frame->registerAction("菜单", brls::BUTTON_X, [this](...) {
        m_menu.title = m_games[m_focusedIndex.load()].displayName;
        m_menu.show();
        return true;
    });
}

void Home::startNacpLoader() {
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
        if (!token.stop_requested()) {
            brls::sync([this]() {
                m_jsonCache.save();
                m_frame->setActionAvailable(brls::BUTTON_Y, true);
            });
        }
    });
}

void Home::applyMetadata(int gameIdx, const GameMetadata& meta) {
    std::string appIdKey = format::appIdHex(m_games[gameIdx].appId);

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
