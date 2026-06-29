/**
 * StoreGameList - 商店游戏列表页面实现
 */

#include "activity/storeGameList.hpp"
#include <borealis/core/i18n.hpp>
#include "dataSource/storeGameListDS.hpp"
#include "view/storeGameCard.hpp"
#include "view/customDialog.hpp"
#include "view/qrCodeView.hpp"
#include "activity/storeModList.hpp"
#include "utils/webpDecoder.hpp"
#include "core/audio.hpp"
#include "utils/pageNav.hpp"
#include "view/keyboardInput.hpp"
#include "utils/format.hpp"
#include "common/config.hpp"
#include <algorithm>

StoreGameList::StoreGameList(GameManager& gameManager)
    : m_gameManager(gameManager) {}

StoreGameList::~StoreGameList() {
    m_stopSource.request_stop();
    m_iconFileCache.save();

    // 释放游戏图标纹理
    NVGcontext* vg = brls::Application::getNVGContext();
    for (auto& icon : m_manager.iconCache()) {
        if (icon.second > 0) nvgDeleteImage(vg, icon.second);
    }
}

void StoreGameList::onContentAvailable() {
    m_emptyHint->setVisibility(brls::Visibility::GONE);
    m_manager.cacheInstalledTids();
    m_iconFileCache.load();
    setupGrid();
    setupSearch();
    setupFilterMenu();
    showPlaceholders();

    // B 键：有搜索词时重置搜索，否则返回主页
    m_frame->registerAction(brls::getStr("views/myframe/back"), brls::BUTTON_B, [this](...) {
        return handleBackOrResetSearch();
    });

    // ZR：返回主页（隐藏 hint）
    m_frame->registerAction("", brls::BUTTON_RT, [this](...) {
        Audio::instance()->play(SoundEffect::Enter);
        pageNav::pop(pageNav::Anim::SLIDE_RIGHT);
        return true;
    }, true);

    // + 键：上传模组
    m_frame->registerAction(brls::getStr("storeGameList/uploadMod"), brls::BUTTON_START, [](...) {
        Audio::instance()->play(SoundEffect::Enter);
        QrCodeView::show(config::websiteUrl, config::websiteUrl);
        return true;
    });

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

bool StoreGameList::handleBackOrResetSearch() {
    if (!m_keyword.empty()) {
        Audio::instance()->play(SoundEffect::Click);
        m_keyword.clear();
        reloadData();
    } else {
        Audio::instance()->play(SoundEffect::Enter);
        pageNav::pop(pageNav::Anim::SLIDE_RIGHT);
    }
    return true;
}

void StoreGameList::setupSearch() {
    m_frame->registerAction(brls::getStr("storeGameList/searchAction"), brls::BUTTON_BACK, [this](...) {
        Audio::instance()->play(SoundEffect::Enter);
        KeyboardInput::show([this](std::string result) {
            if (result == m_keyword) return;
            m_keyword = result;
            reloadData();
        }, brls::getStr("storeGameList/searchPlaceholder"), 50);
        return true;
    });
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
        if (list[i].iconId != 0) continue;

        int cachedIcon = m_manager.getCachedIcon(list[i].gameTid);
        if (cachedIcon != 0) {
            list[i].iconId = cachedIcon;
            applyVisibleIcon(i, cachedIcon);
            continue;
        }

        candidates.push_back({std::abs(i - center), i});
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
            if (token.stop_requested()) return;
            auto data = StoreGameIconCache::readIcon(tid);
            if (token.stop_requested()) return;

            brls::sync([this, gameIdx, tid, data = std::move(data), token]() mutable {
                if (token.stop_requested()) return;
                onLocalIconLoaded(gameIdx, tid, std::move(data));
            });
        }, token);
    }
}

void StoreGameList::applyVisibleIcon(int index, int textureId) {
    if (textureId <= 0) return;
    auto* cell = m_grid->getGridItemByIndex(index);
    if (cell) static_cast<StoreGameCard*>(cell)->setIcon(textureId);
}

int StoreGameList::createIconTexture(const std::vector<uint8_t>& data) {
    auto img = webpDecoder::decode(data.data(), data.size());
    if (img.width <= 0) return 0;
    return nvgCreateImageRGBA(brls::Application::getNVGContext(), img.width, img.height, 0, img.pixels.data());
}

void StoreGameList::onLocalIconLoaded(int gameIdx, std::string tid, std::vector<uint8_t> data) {
    bool hasLocalIcon = false;

    if (!data.empty()) {
        int textureId = createIconTexture(data);
        if (textureId > 0) {
            auto& game = m_manager.storeGameList()[gameIdx];
            game.iconId = textureId;
            m_manager.cacheIcon(tid, textureId);
            applyVisibleIcon(gameIdx, textureId);
            hasLocalIcon = true;
        }
    }

    api::game::IconCacheValidator validator;
    validator.hasLocalFile = hasLocalIcon;
    if (hasLocalIcon) {
        auto metadata = m_iconFileCache.metadata(tid);
        validator.etag = std::move(metadata.etag);
        validator.lastModified = std::move(metadata.lastModified);
    }

    submitIconNetwork(gameIdx, std::move(tid), std::move(validator));
}

void StoreGameList::submitIconNetwork(int gameIdx, std::string tid, api::game::IconCacheValidator validator) {
    auto token = m_stopSource.get_token();

    ThreadPool::instance().submit([this, gameIdx, tid = std::move(tid), validator = std::move(validator)](std::stop_token token) {
        auto result = api::game::fetchIcon(tid, validator, token);
        if (token.stop_requested()) return;

        bool savedToFile = false;
        if (result.success && result.hasData) {
            savedToFile = StoreGameIconCache::writeIcon(tid, result.data);
        }
        if (token.stop_requested()) return;

        brls::sync([this, gameIdx, result = std::move(result), savedToFile, token]() mutable {
            if (token.stop_requested()) return;
            onIconDownloaded(gameIdx, std::move(result), savedToFile);
        });
    }, token);
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

void StoreGameList::onIconDownloaded(int gameIdx, api::game::IconResult result, bool savedToFile) {
    m_pendingIcons--;
    auto& game = m_manager.storeGameList()[gameIdx];

    if (result.notModified) {
        submitNextIcons(1);
        return;
    }

    if (!result.success || !result.hasData) {
        if (game.iconId <= 0) {
            game.iconId = -1;
            m_manager.cacheIcon(game.gameTid, -1);
        }
        submitNextIcons(1);
        return;
    }

    if (savedToFile) {
        m_iconFileCache.updateMetadata(game.gameTid, result.etag, result.lastModified);
    }

    if (game.iconId > 0) {
        submitNextIcons(1);
        return;
    }

    int textureId = createIconTexture(result.data);
    game.iconId = (textureId > 0) ? textureId : -1;
    m_manager.cacheIcon(game.gameTid, game.iconId);
    applyVisibleIcon(gameIdx, textureId);

    submitNextIcons(1);
}
