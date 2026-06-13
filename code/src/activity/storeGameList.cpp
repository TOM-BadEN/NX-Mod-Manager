/**
 * StoreGameList - 商店游戏列表页面实现
 */

#include "activity/storeGameList.hpp"
#include <borealis/core/i18n.hpp>
#include "dataSource/storeGameListDS.hpp"
#include "view/storeGameCard.hpp"
#include "view/customDialog.hpp"
#include "activity/storeModList.hpp"
#include "utils/webpDecoder.hpp"
#include "core/audio.hpp"
#include "utils/pageNav.hpp"
#include "view/keyboardInput.hpp"
#include "utils/format.hpp"
#include <algorithm>

StoreGameList::StoreGameList(GameManager& gameManager)
    : m_gameManager(gameManager) {}

StoreGameList::~StoreGameList() {
    m_stopSource.request_stop();

    // 释放游戏图标纹理
    NVGcontext* vg = brls::Application::getNVGContext();
    for (auto& game : m_manager.storeGameList()) {
        if (game.iconId > 0) nvgDeleteImage(vg, game.iconId);
    }
}

void StoreGameList::onContentAvailable() {
    m_emptyHint->setVisibility(brls::Visibility::GONE);
    m_manager.cacheInstalledTids();
    setupGrid();
    setupSearch();
    setupFilterMenu();
    showPlaceholders();

    // B 键：返回主页
    m_frame->registerAction(brls::getStr("views/myframe/back"), brls::BUTTON_B, [this](...) {
        Audio::instance()->play(SoundEffect::Enter);
        pageNav::pop(pageNav::Anim::SLIDE_RIGHT);
        return true;
    });

    // ZR：返回主页（隐藏 hint）
    m_frame->registerAction("", brls::BUTTON_RT, [this](...) {
        Audio::instance()->play(SoundEffect::Enter);
        pageNav::pop(pageNav::Anim::SLIDE_RIGHT);
        return true;
    }, true);

    loadNextPage();
}

void StoreGameList::setupGrid() {
    m_grid->setPadding(17, 15, 17, 40);
    m_grid->registerCell("StoreGameCard", StoreGameCard::create);
    m_grid->onNextPage([this] {
        if (m_placeholderMode) return;
        if (!m_loading && m_manager.hasMore()) {
            loadNextPage();
        }
    });
    m_grid->setFocusChangeCallback([this](size_t index) {
        if (m_placeholderMode) return;
        m_focusedIndex = static_cast<int>(index);
        m_frame->setIndexText(std::to_string(index + 1) + " / " + std::to_string(m_manager.total()));
        m_frame->setSubtitle(m_manager.storeGameList()[index].gameTid);
    });
}

void StoreGameList::loadNextPage() {
    m_loading = true;

    // 主线程快照所有参数
    int page = m_manager.currentPage() + 1;
    auto filterMode = m_manager.getFilterMode();
    auto keyword = m_keyword;
    auto tidsJson = m_manager.tidsJson();
    auto token = m_stopSource.get_token();

    ThreadPool::instance().submit([this, page, filterMode, keyword, tidsJson](std::stop_token token) {
        api::game::GameListResult result;
        if (filterMode == GameFilterMode::Installed) result = api::game::fetchInstalledGameList(page, 20, keyword, tidsJson, true, token);
        else if (filterMode == GameFilterMode::NotInstalled) result = api::game::fetchInstalledGameList(page, 20, keyword, tidsJson, false, token);
        else result = api::game::fetchGameList(page, 20, keyword, token);
        if (token.stop_requested()) return;

        brls::sync([this, result = std::move(result), token]() mutable {
            if (token.stop_requested()) return;
            onPageLoaded(std::move(result));
        });
    }, token);
}

void StoreGameList::setupSearch() {
    m_frame->registerAction(brls::getStr("storeGameList/searchAction"), brls::BUTTON_BACK, [this](...) {
        Audio::instance()->play(SoundEffect::Enter);
        KeyboardInput::show([this](std::string result) {
            if (result == m_keyword) return;
            m_keyword = result;
            m_manager.setKeyword(m_keyword);
            m_frame->setActionAvailable(brls::BUTTON_Y, !m_keyword.empty());
            reloadData();
        }, brls::getStr("storeGameList/searchPlaceholder"), 50);
        return true;
    });

    m_frame->registerAction(brls::getStr("storeGameList/resetSearch"), brls::BUTTON_Y, [this](...) {
        Audio::instance()->play(SoundEffect::Click);
        m_keyword.clear();
        m_manager.setKeyword("");
        m_frame->setActionAvailable(brls::BUTTON_Y, false);
        reloadData();
        return true;
    });
    m_frame->setActionAvailable(brls::BUTTON_Y, false);
}

void StoreGameList::setupFilterMenu() {
    m_filterMenu.title = brls::getStr("storeGameList/filterTitle");
    m_filterMenu.shouldShowFakeHighlight = [this]{ return !m_manager.storeGameList().empty(); };

    auto addFilterItem = [this](const std::string& key, const std::string& descKey, GameFilterMode mode) {
        auto& item = m_filterMenu.addItem(brls::getStr(key), brls::getStr(descKey));
        item.setBadge([this, mode]{ return m_manager.getFilterMode() == mode ? std::string("\uE14B") : std::string(); });
        item.setAction([this, mode]{
            if (m_manager.getFilterMode() == mode) return;
            m_manager.setFilterMode(mode);
            reloadData();
        });
    };

    addFilterItem("storeGameList/filterAll", "storeGameList/filterAllDesc", GameFilterMode::All);
    addFilterItem("storeGameList/filterInstalled", "storeGameList/filterInstalledDesc", GameFilterMode::Installed);
    addFilterItem("storeGameList/filterNotInstalled", "storeGameList/filterNotInstalledDesc", GameFilterMode::NotInstalled);

    m_frame->registerAction(brls::getStr("storeGameList/filterAction"), brls::BUTTON_X, [this](...) {
        Audio::instance()->play(SoundEffect::Enter);
        m_filterMenu.show();
        return true;
    });
}

void StoreGameList::reloadData() {
    // 取消旧任务，创建新取消源
    m_stopSource.request_stop();
    m_stopSource = std::stop_source{};
    m_pendingIcons = 0;

    // 清空纹理
    NVGcontext* vg = brls::Application::getNVGContext();
    for (auto& game : m_manager.storeGameList()) {
        if (game.iconId > 0) nvgDeleteImage(vg, game.iconId);
    }

    // 重置数据
    m_manager.reset();
    m_grid->setDefaultCellFocus(0);
    showPlaceholders();
    m_loading = false;

    // 更新标题
    m_frame->setTitle(m_keyword.empty() ? brls::getStr("storeGameList/titleDefault") : brls::getStr("storeGameList/titleSearch", m_keyword));
    m_frame->setSubtitle("");

    loadNextPage();
}

void StoreGameList::showPlaceholders() {
    m_placeholderGames.clear();
    m_placeholderGames.reserve(15);
    for (int i = 0; i < 15; i++) {
        api::game::GameList game;
        game.gameName = brls::getStr("storeGameList/placeholderTitle");
        game.gameTid = "";
        game.lastUpdate = "...";
        game.iconId = -1;
        game.modCount = 0;
        game.installed = false;
        m_placeholderGames.push_back(std::move(game));
    }

    m_placeholderMode = true;
    m_emptyHint->setVisibility(brls::Visibility::GONE);
    m_frame->setIndexText(brls::getStr("storeGameList/placeholderIndex"));
    m_frame->setSubtitle("");
    m_frame->setActionAvailable(brls::BUTTON_BACK, false);
    m_frame->setActionAvailable(brls::BUTTON_X, false);
    m_grid->setDataSource(new StoreGameListDS(m_placeholderGames, [](size_t) {}));
    m_grid->setInteractionLocked(true);
    brls::Application::giveFocus(m_grid);
}

void StoreGameList::submitNextIcons(int count) {
    auto& list = m_manager.storeGameList();
    int center = m_focusedIndex;

    // 找离焦点最近的未下载 icon
    std::vector<std::pair<int, int>> candidates;  // {距离, 索引}
    for (int i = 0; i < (int)list.size(); i++) {
        if (list[i].iconId == 0) candidates.push_back({std::abs(i - center), i});
    }
    if (candidates.empty()) return;
    std::sort(candidates.begin(), candidates.end());

    // 限制提交数量（保持并发不超过 3）
    int toSubmit = std::min(count, (int)candidates.size());
    toSubmit = std::min(toSubmit, 3 - m_pendingIcons);
    if (toSubmit <= 0) return;

    auto token = m_stopSource.get_token();

    for (int i = 0; i < toSubmit; i++) {
        int gameIdx = candidates[i].second;
        std::string tid = list[gameIdx].gameTid;
        list[gameIdx].iconId = -2;  // 标记为下载中
        m_pendingIcons++;

        ThreadPool::instance().submit([this, tid, gameIdx](std::stop_token token) {
            auto result = api::game::fetchIcon(tid, token);
            if (token.stop_requested()) return;

            brls::sync([this, gameIdx, result = std::move(result), token]() mutable {
                if (token.stop_requested()) return;
                onIconDownloaded(gameIdx, std::move(result));
            });
        }, token);
    }
}

void StoreGameList::onPageLoaded(api::game::GameListResult result) {
    m_loading = false;
    if (!result.success) {
        auto goBack = [this] {
            CustomDialog::close();
            if (m_manager.storeGameList().empty()) brls::sync([this] { pageNav::pop(pageNav::Anim::SLIDE_RIGHT); });
        };
        CustomDialog::show(result.error, {{brls::getStr("storeGameList/ok"), goBack}}, goBack);
        return;
    }

    m_manager.appendPage(std::move(result));
    auto& list = m_manager.storeGameList();
    if (list.empty()) {
        m_grid->setDataSource(nullptr);
        m_grid->setInteractionLocked(false);
        m_placeholderMode = false;
        m_frame->setActionAvailable(brls::BUTTON_BACK, true);
        m_frame->setActionAvailable(brls::BUTTON_X, true);
        m_emptyHint->setVisibility(brls::Visibility::VISIBLE);
        m_frame->setSubtitle("");
        brls::Application::giveFocus(m_emptyHint);
        return;
    }
    m_emptyHint->setVisibility(brls::Visibility::GONE);

    if (m_placeholderMode) {
        m_grid->setDataSource(new StoreGameListDS(m_manager.storeGameList(), [this](size_t index) {
            auto& game = m_manager.storeGameList()[index];
            std::string version = m_manager.getLocalVersion(index);
            pageNav::push(new StoreModList(game.gameTid, game.gameName, m_gameManager, nullptr, version));
        }));
        m_placeholderMode = false;
        m_grid->setInteractionLocked(false);
        m_frame->setActionAvailable(brls::BUTTON_BACK, true);
        m_frame->setActionAvailable(brls::BUTTON_X, true);
        m_grid->forceRequestNextPage();
        m_grid->setDefaultCellFocus(0);
        m_grid->instantFocus(0);
    } else {
        m_grid->notifyDataChanged();
    }

    submitNextIcons(3);
}

void StoreGameList::onIconDownloaded(int gameIdx, api::game::IconResult result) {
    m_pendingIcons--;

    if (!result.success || result.data.empty()) {
        m_manager.storeGameList()[gameIdx].iconId = -1;
    } else {
        auto img = webpDecoder::decode(result.data.data(), result.data.size());
        int textureId = img.width > 0 ? nvgCreateImageRGBA(brls::Application::getNVGContext(), img.width, img.height, 0, img.pixels.data()) : 0;
        m_manager.storeGameList()[gameIdx].iconId = (textureId > 0) ? textureId : -1;
        if (textureId > 0) {
            auto* cell = m_grid->getGridItemByIndex(gameIdx);
            if (cell) static_cast<StoreGameCard*>(cell)->setIcon(textureId);
        }
    }

    submitNextIcons(1);
}
