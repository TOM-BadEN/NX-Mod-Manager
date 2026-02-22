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

    // 子菜单配置
    m_modeSubMenu = {"选择模式", {
        MenuItemConfig{"模式1", "基础模式"}
            .badge([this] { return m_currentMode == 1 ? "\uE876" : ""; })
            .action([this] { m_currentMode = 1; })
            .popPage(),
        MenuItemConfig{"模式2", "进阶模式"}
            .badge([this] { return m_currentMode == 2 ? "\uE876" : ""; })
            .action([this] { m_currentMode = 2; })
            .popPage(),
        MenuItemConfig{"模式3", "高级模式"}
            .badge([this] { return m_currentMode == 3 ? "\uE876" : ""; })
            .action([this] { m_currentMode = 3; })
            .popPage(),
    }};
    m_modeSubMenu.defaultFocus = [this] { return (size_t)(m_currentMode - 1); };

    // 多选测试子菜单（1-100）
    m_multiSelectTest = {"多选测试", {}};
    m_multiSelectTest.multiSelect = true;
    for (int i = 1; i <= 100; i++) m_multiSelectTest.items.push_back(MenuItemConfig{std::to_string(i), ""});
    m_multiSelectTest.onConfirm = [](const std::vector<int>& indices) {
        std::string msg;
        if (indices.empty()) {
            msg = "未选中任何项";
        } else {
            msg = "选中了：";
            for (size_t i = 0; i < indices.size(); i++) {
                if (i > 0) msg += ", ";
                msg += std::to_string(indices[i] + 1);
            }
        }
        auto* dialog = new brls::Dialog(msg);
        dialog->addButton("确认", []() {});
        dialog->addButton("取消", []() {});
        dialog->open();
    };

    // 测试菜单
    m_testMenu = {"测试菜单", {
        MenuItemConfig{"对话框测试", "点击后关闭菜单并弹出对话框"}
            .action([]() {
                auto* dialog = new brls::Dialog("这是一个测试对话框");
                dialog->addButton("确认", []() {});
                dialog->addButton("取消", []() {});
                dialog->open();
            }),
        MenuItemConfig{"禁用测试", "此项已禁用，不可点击"}
            .disabled(),
        MenuItemConfig{"开关切换测试", "点击切换开启/关闭状态"}
            .badge([this]() { return m_toggleState ? "开启" : "关闭"; })
            .badgeHighlight([this] { return m_toggleState; })
            .action([this]() { m_toggleState = !m_toggleState; })
            .stayOpen(),
        MenuItemConfig{"模式选择", "选择不同的运行模式"}
            .badge([this] { return "模式" + std::to_string(m_currentMode); })
            .submenu(&m_modeSubMenu),
        MenuItemConfig{"多选测试", "1-100多选，+键提交"}
            .submenu(&m_multiSelectTest),
    }};
    m_frame->registerAction("菜单", brls::BUTTON_X, [this](...) {
        m_testMenu.show();
        return true;
    });

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
        brls::Application::pushActivity(new ModList(game.dirPath, game.displayName, game.appId));
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
