/**
 * StoreGameList - 商店游戏列表页面实现
 */

#include "ui/page/storeGameList.hpp"
#include "common/config.hpp"
#include "core/audio.hpp"
#include "core/frameQueue.hpp"
#include "ui/dataSource/storeGameListDS.hpp"
#include "ui/navigation/navigationGroups.hpp"
#include "ui/page/storeModList.hpp"
#include "ui/view/dialog/customDialog.hpp"
#include "ui/view/dialog/keyboardInput.hpp"
#include "ui/view/qrCodeView.hpp"
#include "ui/view/storeGameCard.hpp"
#include "utils/format.hpp"
#include "utils/threadPool.hpp"
#include <borealis/core/cache_helper.hpp>
#include <borealis/core/i18n.hpp>
#include <climits>
#include <cstdlib>
#include <utility>

StoreGameList::StoreGameList(GameManager& gameManager)
    : m_gameManager(gameManager) {
    inflateFromXMLRes("xml/view/page/storeGameList.xml");

    setHeader();
}

StoreGameList::~StoreGameList() {
    m_stopSource.request_stop();
    m_iconFileCache.save();
}

void StoreGameList::setHeader() {
    HeaderState headerState;
    headerState.setNavigation(createMainNavigationState(MainNavigationPage::StoreGameList));
    headerState.setContentTitle(TID_PLACEHOLDER);
    ShellState::setHeaderState(headerState);
}

void StoreGameList::onContentAvailable() {
    m_emptyHint->setVisibility(brls::Visibility::GONE);
    m_manager.cacheInstalledTids();
    m_iconFileCache.load();
    setupGrid();
    setupSearch();
    setupFilterMenu();
    showSkeletons();

    // B 键：有搜索词时重置搜索，否则返回主页
    registerAction("", brls::BUTTON_B, [this](...) {
        return handleBackOrResetSearch();
    }, true);

    // ZL：导航左边界
    registerAction("", brls::BUTTON_LT, [this](...) {
        Audio::instance()->play(SoundEffect::FocusLimit);
        shakeHeaderNav(false);
        return true;
    }, true);

    // ZR：返回主页（隐藏 hint）
    registerAction("", brls::BUTTON_RT, [this](...) {
        Audio::instance()->play(SoundEffect::Enter);
        Page::popPage(PageAnimType::SlideFromRight);
        return true;
    }, true);

    // + 键：上传模组
    registerAction(brls::getStr("page/storeGameList/uploadMod"), brls::BUTTON_START, [](...) {
        Audio::instance()->play(SoundEffect::Enter);
        QrCodeView::show(config::websiteUrl, config::websiteUrl);
        return true;
    });

    loadNextPage();
}

void StoreGameList::setupGrid() {
    m_grid->setPadding(5, 15, 5, 40);
    m_grid->registerCell("StoreGameCard", StoreGameCard::create);
    m_grid->onNextPage([this] {
        if (!m_loading && m_manager.hasMore()) {
            loadNextPage();
        }
    });
    m_grid->setFocusChangeCallback([this](size_t index) {
        m_focusedIndex = static_cast<int>(index);
        if (m_manager.storeGameList().empty()) return;
        ShellState::setIndexText(std::to_string(index + 1) + " / " + std::to_string(m_manager.total()));
        ShellState::setHeaderContentTitle(m_manager.storeGameList()[index].gameTid);
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
        if (token.stop_requested()) return;

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
        Page::popPage(PageAnimType::SlideFromRight);
    }
    return true;
}

void StoreGameList::setupSearch() {
    registerAction(brls::getStr("page/storeGameList/searchAction"), brls::BUTTON_BACK, [this](...) {
        Audio::instance()->play(SoundEffect::Enter);
        KeyboardInput::show([this](std::string result) {
            if (result == m_keyword) return;
            m_keyword = result;
            reloadData();
        }, brls::getStr("page/storeGameList/searchPlaceholder"), 50);
        return true;
    });
}

void StoreGameList::setupFilterMenu() {
    m_filterMenu.setIcon(format::themedIconPath("img/menu/filter"));
    m_filterMenu.setShowFakeHighlight([this]{ return !m_manager.storeGameList().empty(); });

    auto addFilterItem = [this](const std::string& key, const std::string& descKey, const std::string& icon, GameFilterMode mode) {
        auto& item = m_filterMenu.addRadio(brls::getStr(key), brls::getStr(descKey));
        item.setIcon(format::themedIconPath(icon));
        item.setSelected([this, mode]{ return m_manager.getFilterMode() == mode; });
        item.onSelected([this, mode]{
            if (m_manager.getFilterMode() == mode) return;
            m_manager.setFilterMode(mode);
            reloadData();
        });
    };

    addFilterItem("page/storeGameList/filterAll", "page/storeGameList/filterAllDesc", "img/menu/all", GameFilterMode::All);
    addFilterItem("page/storeGameList/filterInstalled", "page/storeGameList/filterInstalledDesc", "img/menu/installed", GameFilterMode::Installed);
    addFilterItem("page/storeGameList/filterNotInstalled", "page/storeGameList/filterNotInstalledDesc", "img/menu/notInstalled", GameFilterMode::NotInstalled);

    registerAction(brls::getStr("page/storeGameList/filterAction"), brls::BUTTON_X, [this](...) {
        Audio::instance()->play(SoundEffect::Enter);
        m_filterMenu.show();
        return true;
    });
}

void StoreGameList::reloadData() {
    // 取消旧任务，创建新取消源
    m_stopSource.request_stop();
    m_stopSource = std::stop_source{};

    // 重置数据
    m_manager.reset();
    m_pendingDownloads.clear();
    m_focusedIndex = 0;
    m_activeDownloads = 0;
    m_loading = false;
    m_iconLoading = false;
    m_iconChecking = false;
    m_grid->setDefaultCellFocus(0);
    showSkeletons();
    loadNextPage();
}

void StoreGameList::showSkeletons() {
    m_emptyHint->setVisibility(brls::Visibility::GONE);
    ShellState::setIndexText("0 / 0");
    ShellState::setHeaderContentTitle(TID_PLACEHOLDER);
    setActionAvailable(brls::BUTTON_BACK, false);
    setActionAvailable(brls::BUTTON_X, false);

    auto onTextureMissing = [this](std::string key) { queueCardReload(std::move(key)); };
    auto onGameSelected = [this](size_t index) { onGameCardClicked(index); };
    auto* dataSource = new StoreGameListDS(m_manager.storeGameList(), 15, onTextureMissing, onGameSelected);
    m_grid->setDataSource(dataSource);
    brls::Application::giveFocus(m_grid);
}

void StoreGameList::startIconLoader() {
    if (m_iconLoading) return;
    m_iconLoading = true;
    submitNextIcon();
}

void StoreGameList::submitNextIcon() {
    auto& list = m_manager.storeGameList();

    // 找离焦点最近的待显示游戏
    size_t gameIdx = list.size();
    int bestDist = INT_MAX;
    for (size_t index = 0; index < list.size(); index++) {
        if (!list[index].isPending || list[index].isLoading) continue;
        int dist = std::abs(static_cast<int>(index) - m_focusedIndex);
        if (dist < bestDist) {
            bestDist = dist;
            gameIdx = index;
        }
    }
    if (gameIdx == list.size()) {
        m_iconLoading = false;
        return;
    }

    std::string tid = list[gameIdx].gameTid;
    list[gameIdx].isLoading = true;
    auto token = m_stopSource.get_token();
    FrameQueue::enqueue(token, [this, tid = std::move(tid)] {
        auto& textureCache = brls::TextureCache::instance();
        std::string key = "S" + tid;
        int iconId = textureCache.getCache(key);
        if (iconId > 0) {
            showCard(tid, iconId);
            scheduleNetworkTasks();
            submitNextIcon();
            return;
        }

        auto token = m_stopSource.get_token();
        ThreadPool::instance().submit([this, tid](std::stop_token token) {
            if (token.stop_requested()) return;

            auto data = StoreGameIconCache::readIcon(tid);
            imageDecoder::DecodedImage image;
            if (!data.empty()) image = imageDecoder::decodeWebp(data.data(), data.size());
            if (token.stop_requested()) return;

            if (image.width > 0 && image.height > 0 && !image.pixels.empty()) {
                FrameQueue::enqueue(token, [this, tid, image = std::move(image)] { applyLocalIcon(tid, image); });
                return;
            }

            brls::sync([this, tid, token] {
                if (token.stop_requested()) return;
                queueIconDownload(tid);
            });
        }, token);
    });
}

void StoreGameList::queueIconDownload(std::string tid) {
    m_pendingDownloads.insert(std::move(tid));
    scheduleNetworkTasks();
    submitNextIcon();
}

void StoreGameList::queueCardReload(std::string key) {
    std::string tid = key.substr(1);
    int gameIdx = m_manager.findByTid(tid);
    auto& game = m_manager.storeGameList()[gameIdx];
    game.iconKey.clear();
    game.isPending = true;
    game.isLoading = false;
    startIconLoader();
}

int StoreGameList::loadGameIcon(const std::string& key, const imageDecoder::DecodedImage& image) {
    auto& textureCache = brls::TextureCache::instance();
    int iconId = textureCache.getCache(key);
    if (iconId > 0 || image.width <= 0 || image.height <= 0 || image.pixels.empty()) return iconId;

    iconId = nvgCreateImageRGBA(brls::Application::getNVGContext(), image.width, image.height, 0, image.pixels.data());
    if (iconId > 0) textureCache.addCache(key, iconId);
    return iconId;
}

void StoreGameList::applyLocalIcon(const std::string& tid, const imageDecoder::DecodedImage& image) {
    int iconId = loadGameIcon("S" + tid, image);
    showCard(tid, iconId);
    scheduleNetworkTasks();
    submitNextIcon();
}

void StoreGameList::showCard(const std::string& tid, int iconId) {
    int gameIdx = m_manager.findByTid(tid);
    auto& game = m_manager.storeGameList()[gameIdx];
    game.iconKey = iconId > 0 ? "S" + tid : "";
    game.isPending = false;
    game.isLoading = false;
    m_grid->reloadItem(static_cast<size_t>(gameIdx));
    if (iconId > 0) brls::TextureCache::instance().removeCache(iconId);
}

void StoreGameList::scheduleNetworkTasks() {
    auto& list = m_manager.storeGameList();
    while (m_activeDownloads + (m_iconChecking ? 1 : 0) < 2) {
        std::string downloadTid;
        int bestDist = INT_MAX;
        for (size_t index = 0; index < list.size(); index++) {
            if (m_pendingDownloads.count(list[index].gameTid) == 0) continue;
            int dist = std::abs(static_cast<int>(index) - m_focusedIndex);
            if (dist < bestDist) {
                bestDist = dist;
                downloadTid = list[index].gameTid;
            }
        }

        if (!downloadTid.empty()) {
            m_pendingDownloads.erase(downloadTid);
            m_activeDownloads++;
            submitIconDownload(std::move(downloadTid));
            continue;
        }

        if (m_iconChecking) return;

        std::string checkTid;
        bestDist = INT_MAX;
        for (size_t index = 0; index < list.size(); index++) {
            auto& game = list[index];
            if (game.isPending || game.iconKey.empty() || m_checkedIcons.count(game.gameTid) > 0) continue;
            int dist = std::abs(static_cast<int>(index) - m_focusedIndex);
            if (dist < bestDist) {
                bestDist = dist;
                checkTid = game.gameTid;
            }
        }
        if (checkTid.empty()) return;

        m_checkedIcons.insert(checkTid);
        m_iconChecking = true;
        auto metadata = m_iconFileCache.metadata(checkTid);
        api::game::IconCacheValidator validator;
        validator.hasLocalFile = true;
        validator.etag = std::move(metadata.etag);
        validator.lastModified = std::move(metadata.lastModified);
        submitIconValidation(std::move(checkTid), std::move(validator));
    }
}

void StoreGameList::submitIconDownload(std::string tid) {
    auto token = m_stopSource.get_token();
    ThreadPool::instance().submit([this, tid = std::move(tid)](std::stop_token token) {
        if (token.stop_requested()) return;

        auto result = api::game::fetchIcon(tid, {}, token);
        bool success = result.success && result.hasData;
        imageDecoder::DecodedImage image;
        if (success) {
            StoreGameIconCache::writeIcon(tid, result.data);
            image = imageDecoder::decodeWebp(result.data.data(), result.data.size());
        }
        if (token.stop_requested()) return;

        std::string etag = std::move(result.etag);
        std::string lastModified = std::move(result.lastModified);
        brls::sync([this, tid = std::move(tid), success, etag = std::move(etag), lastModified = std::move(lastModified), image = std::move(image), token]() mutable {
            if (token.stop_requested()) return;
            if (success) m_iconFileCache.updateMetadata(tid, etag, lastModified);
            FrameQueue::enqueue(token, [this, tid = std::move(tid), success, image = std::move(image)] { applyDownloadResult(tid, success, image); });
        });
    }, token);
}

void StoreGameList::submitIconValidation(std::string tid, api::game::IconCacheValidator validator) {
    auto token = m_stopSource.get_token();
    ThreadPool::instance().submit([this, tid = std::move(tid), validator = std::move(validator)](std::stop_token token) {
        if (token.stop_requested()) return;

        auto result = api::game::fetchIcon(tid, validator, token);
        bool hasUpdate = result.success && result.hasData;
        if (hasUpdate) StoreGameIconCache::writeIcon(tid, result.data);
        if (token.stop_requested()) return;

        std::string etag = std::move(result.etag);
        std::string lastModified = std::move(result.lastModified);
        brls::sync([this, tid = std::move(tid), hasUpdate, etag = std::move(etag), lastModified = std::move(lastModified), token] {
            if (token.stop_requested()) return;
            if (hasUpdate) m_iconFileCache.updateMetadata(tid, etag, lastModified);
            m_iconChecking = false;
            scheduleNetworkTasks();
        });
    }, token);
}

void StoreGameList::applyDownloadResult(const std::string& tid, bool success, const imageDecoder::DecodedImage& image) {
    int iconId = success ? loadGameIcon("S" + tid, image) : 0;
    showCard(tid, iconId);
    if (success) m_checkedIcons.insert(tid);
    m_activeDownloads--;
    scheduleNetworkTasks();
}

void StoreGameList::onPageLoaded(api::game::GameListResult result) {
    m_loading = false;
    if (!result.success) {
        auto goBack = [this] {
            CustomDialog::close();
            if (m_manager.storeGameList().empty()) brls::sync([this] { Page::popPage(PageAnimType::SlideFromRight); });
        };
        CustomDialog::show(result.error, {{brls::getStr("page/storeGameList/ok"), goBack}}, goBack);
        return;
    }

    bool firstPage = m_manager.currentPage() == 0;
    m_manager.appendPage(std::move(result));
    auto& list = m_manager.storeGameList();
    if (list.empty()) {
        m_grid->setDataSource(nullptr);
        setActionAvailable(brls::BUTTON_BACK, true);
        setActionAvailable(brls::BUTTON_X, true);
        m_emptyHint->setVisibility(brls::Visibility::VISIBLE);
        ShellState::setHeaderContentTitle(TID_PLACEHOLDER);
        brls::Application::giveFocus(m_emptyHint);
        return;
    }
    m_emptyHint->setVisibility(brls::Visibility::GONE);

    if (firstPage) {
        size_t focusIndex = m_grid->getDefaultCellFocus();
        if (focusIndex >= list.size()) focusIndex = list.size() - 1;
        m_grid->setDefaultCellFocus(focusIndex);
        m_grid->reloadData();
        m_grid->forceRequestNextPage();
        setActionAvailable(brls::BUTTON_BACK, true);
        setActionAvailable(brls::BUTTON_X, true);
        m_grid->instantFocus(focusIndex);
    } else {
        m_grid->notifyDataChanged();
    }

    startIconLoader();
}

void StoreGameList::onGameCardClicked(size_t index) {
    auto& game = m_manager.storeGameList()[index];
    std::string version = m_manager.getLocalVersion(index);

    Page::pushPage(new StoreModList(game.gameTid, game.gameName, game.iconKey, m_gameManager, nullptr, version));
}
