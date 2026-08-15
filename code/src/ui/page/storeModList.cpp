/**
 * StoreModList - 商店模组列表页面实现
 */

#include "ui/page/storeModList.hpp"
#include "common/config.hpp"
#include "common/modInfo.hpp"
#include "core/audio.hpp"
#include "core/frameQueue.hpp"
#include "ui/core/pageHost.hpp"
#include "ui/dataSource/storeModListDS.hpp"
#include "ui/navigation/navigationGroups.hpp"
#include "ui/page/storeGameList.hpp"
#include "ui/page/storeModDetail.hpp"
#include "ui/view/dialog/customDialog.hpp"
#include "ui/view/dialog/keyboardInput.hpp"
#include "ui/view/qrCodeView.hpp"
#include "ui/view/storeModCard.hpp"
#include "utils/format.hpp"
#include <borealis/core/i18n.hpp>
#include <climits>
#include <cstdlib>
#include <utility>

// ── 筛选选项定义（展示层数据） ──

struct FilterOption {
    std::string value; // API 值
    std::string label; // 显示名
    std::string hint;  // 菜单提示（可为空）
};

static const std::vector<FilterOption>& sortOptions() {
    static const std::vector<FilterOption> options = {
        {"latest", brls::getStr("page/storeModList/sortLatest"), brls::getStr("page/storeModList/sortLatestDesc")},
        {"download", brls::getStr("page/storeModList/sortDownload"), brls::getStr("page/storeModList/sortDownloadDesc")},
        {"like", brls::getStr("page/storeModList/sortLike"), brls::getStr("page/storeModList/sortLikeDesc")},
    };
    return options;
}

static const std::vector<FilterOption>& modTypeFilterOptions() {
    static const std::vector<FilterOption> options = [] {
        std::vector<FilterOption> v;
        v.push_back({"", brls::getStr("page/storeModList/typeAll"), brls::getStr("page/storeModList/typeAllDesc")});
        for (auto& opt : modTypeOptions())
            v.push_back({opt.value, opt.label, opt.desc});
        return v;
    }();
    return options;
}

static const char* sortLabel(const std::string& sort) {
    for (auto& option : sortOptions()) {
        if (option.value == sort) return option.label.c_str();
    }
    return sortOptions().front().label.c_str();
}

static std::string sortIcon(const std::string& sort) {
    if (sort == "download") return format::themedIconPath("img/menu/download");
    if (sort == "like") return format::themedIconPath("img/menu/like");
    return format::themedIconPath("img/menu/time");
}

static const char* modTypeLabel(const std::string& modType) {
    for (auto& option : modTypeFilterOptions()) {
        if (option.value == modType) return option.label.c_str();
    }
    return modTypeFilterOptions().front().label.c_str();
}

static std::string menuModTypeIcon(const std::string& modType) {
    if (modType.empty()) return format::themedIconPath("img/menu/all");
    if (modType == "performance") return format::themedIconPath("img/menu/fps");
    if (modType == "graphics") return format::themedIconPath("img/menu/hd");
    if (modType == "translation") return format::themedIconPath("img/menu/language");
    if (modType == "feature") return format::themedIconPath("img/menu/otherSetting");
    if (modType == "music") return format::themedIconPath("img/menu/music");
    if (modType == "ui") return format::themedIconPath("img/menu/theme");
    if (modType == "skin") return format::themedIconPath("img/menu/skin");
    if (modType == "cheat") return format::themedIconPath("img/menu/cheats");
    return format::themedIconPath("img/menu/other");
}

static std::string versionLabel(const std::string& version) {
    if (version.empty()) return brls::getStr("page/storeModList/versionAll");
    if (version == "0") return brls::getStr("page/storeModList/versionUniversal");
    return version;
}

StoreModList::StoreModList(std::string gameTid, std::string gameName, std::string gameIconKey, GameManager& gameManager, ModManager* localModManager, std::string gameVersion, bool fromModList)
    : m_manager(std::move(gameTid)), m_gameName(std::move(gameName)), m_gameIconKey(std::move(gameIconKey)), m_gameManager(gameManager), m_localModManager(localModManager), m_gameVersion(std::move(gameVersion)), m_fromModList(fromModList) {
    inflateFromXMLRes("xml/view/page/storeModList.xml");

    setHeader();
}

StoreModList::~StoreModList() {
    m_queryStopSource.request_stop();
    m_pageStopSource.request_stop();
}

TitleState StoreModList::getHeaderTitle() const {
    TitleState titleState;
    titleState.iconTextureKey = m_gameIconKey;
    titleState.iconPath = config::defaultGameIconResource;
    titleState.title = m_manager.getKeyword().empty() ? m_gameName : brls::getStr("page/storeModList/titleSearch", m_manager.getKeyword());
    titleState.subtitle = brls::getStr("page/storeModList/notInstalled");
    if (m_gameVersion != "...") titleState.subtitle = brls::getStr("page/storeModList/localVersion", format::cleanVersion(m_gameVersion));
    return titleState;
}

void StoreModList::setHeader() {
    HeaderState headerState;
    if (isFromModList()) headerState.setNavigation(createModNavigationState(ModNavigationPage::StoreModList));
    headerState.setTitle(getHeaderTitle());
    ShellState::setHeaderState(headerState);
}

void StoreModList::setHeaderTitle() {
    ShellState::setHeaderTitle(getHeaderTitle());
}

void StoreModList::setupNavigationActions() {
    if (!isFromModList()) return;

    // ZL：导航左边界
    registerAction("", brls::BUTTON_LT, [this](...) {
        Audio::instance()->play(SoundEffect::FocusLimit);
        shakeHeaderNav(false);
        return true;
    }, true);

    // ZR：快捷返回 ModList
    registerAction("", brls::BUTTON_RT, [this](...) {
        Audio::instance()->play(SoundEffect::Enter);
        Page::popPage(PageAnimType::SlideFromRight);
        return true;
    }, true);
}

void StoreModList::onContentAvailable() {
    setupGrid();
    setupSearch();
    setupFilterMenu();
    showSkeletons();
    loadVersionMenu();
    startLocalModManagerTask();

    // B 键：有搜索词时重置搜索，否则返回
    registerAction("", brls::BUTTON_B, [this](...) {
        return handleBackOrResetSearch();
    }, true);

    setupNavigationActions();

    // + 键：上传模组
    registerAction(brls::getStr("page/storeModList/uploadMod"), brls::BUTTON_START, [](...) {
        Audio::instance()->play(SoundEffect::Enter);
        QrCodeView::show(config::websiteUrl, config::websiteUrl);
        return true;
    });

    loadNextPage();
}

void StoreModList::startLocalModManagerTask() {
    if (m_localModManager) {
        m_localManagerReady = true;
        return;
    }

    auto token = m_pageStopSource.get_token();
    m_localManagerTask = ThreadPool::instance().submitWaitable([this](std::stop_token token) {
        if (token.stop_requested()) return;
        prepareLocalModManager();
        if (token.stop_requested()) return;
        brls::sync([this, token] {
            if (token.stop_requested()) return;
            onLocalModManagerReady();
        });
    }, token);
}

void StoreModList::prepareLocalModManager() {
    if (m_localModManager) return;

    uint64_t appId = format::appIdFromHex(m_manager.gameTid());
    int idx = m_gameManager.findByAppId(appId);
    if (idx < 0) return;

    m_pageModManager.emplace(m_gameManager.games()[idx]);
    m_localModManager = &m_pageModManager.value();
}

void StoreModList::onLocalModManagerReady() {
    m_localManagerReady = true;
    tryFinishInitialLoad();
}

bool StoreModList::isFromModList() const {
    return m_fromModList;
}

void StoreModList::setupGrid() {
    m_grid->setPadding(5, 15, 5, 40);
    m_grid->registerCell("StoreModCard", StoreModCard::create);
    m_grid->onNextPage([this] {
        if (!m_loading && m_manager.hasMore()) loadNextPage();
    });
    m_grid->setFocusChangeCallback([this](size_t index) {
        m_focusedIndex = static_cast<int>(index);
        if (m_manager.storeModList().empty()) return;
        ShellState::setIndexText(std::to_string(index + 1) + " / " + std::to_string(m_manager.total()));
    });
}

void StoreModList::setQueryActionsAvailable(bool available) {
    setActionAvailable(brls::BUTTON_BACK, available);
    setActionAvailable(brls::BUTTON_X, available);
}

void StoreModList::loadNextPage() {
    m_loading = true;

    // 主线程快照所有参数
    auto gameTid = m_manager.gameTid();
    int page = m_manager.currentPage() + 1;
    auto sort = m_manager.getSort();
    auto keyword = m_manager.getKeyword();
    auto version = m_manager.getVersion();
    auto modType = m_manager.getModType();
    auto token = m_queryStopSource.get_token();

    ThreadPool::instance().submit([this, gameTid, page, sort, keyword, version, modType](std::stop_token token) {
        if (token.stop_requested()) return;

        auto result = api::mod::fetchModList(gameTid, page, 20, sort, keyword, version, modType, token);
        if (token.stop_requested()) return;
        brls::sync([this, page, result = std::move(result), token]() mutable {
            if (token.stop_requested()) return;
            if (page == 1 && !m_localManagerReady) {
                m_pendingFirstPage = std::move(result);
                return;
            }
            onPageLoaded(std::move(result), token);
        });
    }, token);
}

void StoreModList::onPageLoaded(api::mod::ModListResult result, std::stop_token token) {
    m_loading = false;

    if (!result.success) {
        auto goBack = [this, token] {
            CustomDialog::close();
            if (m_manager.storeModList().empty()) {
                brls::sync([this, token] {
                    if (token.stop_requested()) return;
                    Page::popPage(PageAnimType::SlideFromRight);
                });
            }
        };
        CustomDialog::show(result.error, {{brls::getStr("page/storeModList/ok"), goBack}}, goBack);
        return;
    }

    bool firstPage = m_manager.currentPage() == 0;
    size_t oldSize = m_manager.storeModList().size();
    m_manager.appendPage(std::move(result));
    auto& storeMods = m_manager.storeModList();
    for (size_t i = oldSize; i < storeMods.size(); i++) {
        applyLocalState(storeMods[i]);
    }
    if (storeMods.empty()) {
        m_grid->setDataSource(nullptr);
        m_emptyHint->setText(brls::getStr("page/storeModList/noMods"));
        setQueryActionsAvailable(true);
        m_emptyHint->setVisibility(brls::Visibility::VISIBLE);
        ShellState::setIndexText("0 / 0");
        brls::Application::giveFocus(m_emptyHint);
        return;
    }
    m_emptyHint->setVisibility(brls::Visibility::GONE);

    if (firstPage) {
        m_grid->setFastSkeletonMode(true); // 数据到达：进入快速模式，卡片逐张出现
        size_t focusIndex = m_grid->getDefaultCellFocus();
        if (focusIndex >= storeMods.size()) focusIndex = storeMods.size() - 1;
        m_grid->setDefaultCellFocus(focusIndex);
        m_grid->reloadData();
        m_grid->forceRequestNextPage();
        setQueryActionsAvailable(true);
        m_grid->instantFocus(focusIndex);
    } else {
        m_grid->notifyDataChanged();
    }

    startCardLoader();
}

void StoreModList::applyLocalState(api::mod::ModList& mod) {
    mod.downloaded = false;
    mod.hasUpdate = false;

    if (!m_localModManager) return;

    int idx = m_localModManager->findByModID(mod.modId);
    if (idx < 0) return;

    mod.downloaded = true;

    auto& localMod = m_localModManager->mods()[idx];
    mod.hasUpdate = !localMod.fileCrc32.empty() && !mod.fileCrc32.empty() && localMod.fileCrc32 != mod.fileCrc32;
}

void StoreModList::openDetail(size_t index) {
    auto& mod = m_manager.storeModList()[index];
    auto onReturn = [this, index](int likes, int dislikes, int downloads, bool downloaded) {
        auto& mod = m_manager.storeModList()[index];
        mod.likeCount = likes;
        mod.dislikeCount = dislikes;
        mod.downloadCount = downloads;
        mod.downloaded = downloaded;
        if (downloaded) {
            prepareLocalModManager();
            applyLocalState(mod);
        }
        auto* cell = m_grid->getGridItemByIndex(index);
        if (cell) static_cast<StoreModCard*>(cell)->setMod(mod.modName, mod.uploadTime, mod.modType, mod.gameVersion, likes, dislikes, downloads, mod.downloaded, mod.hasUpdate);
    };
    Page::pushPage(new StoreModDetail(mod.modId, m_manager.gameTid(), m_gameName, m_gameIconKey, m_gameManager, onReturn, m_localModManager, m_gameVersion, m_fromModList, false));
}

void StoreModList::reloadData() {
    // 取消旧查询和卡片任务，版本列表请求不受影响
    m_queryStopSource.request_stop();
    m_queryStopSource = std::stop_source{};
    m_pendingFirstPage.reset();

    // 重置数据
    m_manager.reset();
    m_focusedIndex = 0;
    m_loading = false;
    m_cardLoading = false;
    m_grid->setDefaultCellFocus(0);
    m_grid->setFastSkeletonMode(false); // 等待阶段恢复标准骨架提示
    showSkeletons();
    loadNextPage();
}

void StoreModList::showSkeletons() {
    m_emptyHint->setVisibility(brls::Visibility::GONE);
    ShellState::setIndexText("0 / 0");
    setQueryActionsAvailable(false);

    auto onModSelected = [this](size_t index) { openDetail(index); };
    auto* dataSource = new StoreModListDS(m_manager.storeModList(), 1, onModSelected);
    m_grid->setDataSource(dataSource);
    brls::Application::giveFocus(m_grid);
}

void StoreModList::tryFinishInitialLoad() {
    if (!m_localManagerReady || !m_pendingFirstPage) return;

    auto token = m_queryStopSource.get_token();
    auto result = std::move(m_pendingFirstPage.value());
    m_pendingFirstPage.reset();
    onPageLoaded(std::move(result), token);
}

void StoreModList::startCardLoader() {
    if (m_cardLoading) return;
    m_cardLoading = true;
    submitNextCard();
}

void StoreModList::submitNextCard() {
    auto& list = m_manager.storeModList();

    // 找离焦点最近的待显示模组
    size_t modIdx = list.size();
    int bestDist = INT_MAX;
    for (size_t index = 0; index < list.size(); index++) {
        if (!list[index].isPending) continue;
        int dist = std::abs(static_cast<int>(index) - m_focusedIndex);
        if (dist < bestDist) {
            bestDist = dist;
            modIdx = index;
        }
    }
    if (modIdx == list.size()) {
        m_cardLoading = false;
        return;
    }

    auto token = m_queryStopSource.get_token();
    FrameQueue::enqueue(token, [this, modIdx] {
        showCard(modIdx);
    });
}

void StoreModList::showCard(size_t index) {
    m_manager.storeModList()[index].isPending = false;
    m_grid->reloadItem(index);
    submitNextCard();
}

bool StoreModList::handleBackOrResetSearch() {
    if (!m_manager.getKeyword().empty()) {
        Audio::instance()->play(SoundEffect::Click);
        m_manager.setKeyword("");
        setHeaderTitle();
        reloadData();
    } else {
        Audio::instance()->play(SoundEffect::Enter);
        auto* pageHost = dynamic_cast<PageHost*>(getParent());
        auto* previousPage = pageHost->getPreviousPage();
        if (dynamic_cast<StoreGameList*>(previousPage)) Page::popPage();
        else Page::popPage(PageAnimType::SlideFromRight);
    }
    return true;
}

void StoreModList::setupSearch() {
    registerAction(brls::getStr("page/storeModList/searchAction"), brls::BUTTON_BACK, [this](...) {
        Audio::instance()->play(SoundEffect::Enter);
        KeyboardInput::show([this](std::string result) {
            if (result == m_manager.getKeyword()) return;
            m_manager.setKeyword(result);
            setHeaderTitle();
            reloadData();
        }, brls::getStr("page/storeModList/searchPlaceholder"), 50);
        return true;
    });
}

void StoreModList::setupFilterMenu() {
    m_filterMenu.setIcon(format::themedIconPath("img/menu/filter"));
    m_sortMenu.setIcon(format::themedIconPath("img/menu/sortType"));
    m_modTypeMenu.setIcon(format::themedIconPath("img/menu/type"));
    m_versionMenu.setIcon(format::themedIconPath("img/menu/hVerson"));

    // ── 排序子菜单 ──
    for (auto& option : sortOptions()) {
        auto& item = m_sortMenu.addRadio(option.label, option.hint);
        item.setIcon(sortIcon(option.value));
        item.setSelected([this, value = option.value]{ return m_manager.getSort() == value; });
        item.onSelected([this, value = option.value]{
            if (m_manager.getSort() == value) return;
            m_manager.setSort(value);
            m_filterDirty = true;
        });
        item.setBack();
    }

    // ── 模组类型子菜单 ──
    for (auto& option : modTypeFilterOptions()) {
        auto& item = m_modTypeMenu.addRadio(option.label, option.hint);
        item.setIcon(menuModTypeIcon(option.value));
        item.setSelected([this, value = option.value]{ return m_manager.getModType() == value; });
        item.onSelected([this, value = option.value]{
            if (m_manager.getModType() == value) return;
            m_manager.setModType(value);
            m_filterDirty = true;
        });
        item.setBack();
    }

    // ── 筛选主菜单 ──
    m_filterMenu.setShowFakeHighlight([this]{ return !m_manager.storeModList().empty(); });
    m_filterMenu.setOnDismiss([this]{
        if (m_filterDirty) {
            m_filterDirty = false;
            reloadData();
        }
    });

    auto& sortEntry = m_filterMenu.addSubmenu(brls::getStr("page/storeModList/sortEntry"), brls::getStr("page/storeModList/sortEntryDesc"));
    sortEntry.setIcon(format::themedIconPath("img/menu/sortType"));
    sortEntry.setBadge([this]{ return sortLabel(m_manager.getSort()); });
    sortEntry.setPage(m_sortMenu);

    auto& typeEntry = m_filterMenu.addSubmenu(brls::getStr("page/storeModList/typeEntry"), brls::getStr("page/storeModList/typeEntryDesc"));
    typeEntry.setIcon(format::themedIconPath("img/menu/type"));
    typeEntry.setBadge([this]{ return modTypeLabel(m_manager.getModType()); });
    typeEntry.setPage(m_modTypeMenu);

    auto& versionEntry = m_filterMenu.addSubmenu(brls::getStr("page/storeModList/versionEntry"), brls::getStr("page/storeModList/versionEntryDesc"));
    versionEntry.setIcon(format::themedIconPath("img/menu/hVerson"));
    versionEntry.setBadge([this]{ return versionLabel(m_manager.getVersion()); });
    versionEntry.setDisabled([this]{ return !m_versionsLoaded; });
    versionEntry.setPage(m_versionMenu);

    auto& resetItem = m_filterMenu.addAction(brls::getStr("page/storeModList/resetFilter"), brls::getStr("page/storeModList/resetFilterDesc"));
    resetItem.setIcon(format::themedIconPath("img/menu/restoreTitle"));
    resetItem.setBadge("\uE14A");
    resetItem.setDisabled([this]{
        return m_manager.getSort() == "latest" && m_manager.getModType().empty() && m_manager.getVersion().empty();
    });
    resetItem.onSelected([this]{
        m_manager.resetFilter();
        m_filterDirty = true;
    });
    resetItem.setBack();

    registerAction(brls::getStr("page/storeModList/filterAction"), brls::BUTTON_X, [this](...) {
        Audio::instance()->play(SoundEffect::Enter);
        m_filterMenu.show();
        return true;
    });
}

void StoreModList::loadVersionMenu() {
    auto gameTid = m_manager.gameTid();
    auto token = m_pageStopSource.get_token();
    ThreadPool::instance().submit([this, gameTid](std::stop_token token) {
        if (token.stop_requested()) return;

        auto result = api::mod::fetchModGameVersions(gameTid, token);
        if (token.stop_requested()) return;
        brls::sync([this, result = std::move(result), token]() mutable {
            if (token.stop_requested()) return;
            if (!result.success) return;

            // 静态项
            auto& allItem = m_versionMenu.addRadio(brls::getStr("page/storeModList/versionAll"), brls::getStr("page/storeModList/versionAllDesc"));
            allItem.setIcon(format::themedIconPath("img/menu/all"));
            allItem.setSelected([this]{ return m_manager.getVersion().empty(); });
            allItem.onSelected([this]{
                if (m_manager.getVersion().empty()) return;
                m_manager.setVersion("");
                m_filterDirty = true;
            });
            allItem.setBack();

            auto& universalItem = m_versionMenu.addRadio(brls::getStr("page/storeModList/versionUniversal"), brls::getStr("page/storeModList/versionUniversalDesc"));
            universalItem.setIcon(format::themedIconPath("img/menu/common"));
            universalItem.setSelected([this]{ return m_manager.getVersion() == "0"; });
            universalItem.onSelected([this]{
                if (m_manager.getVersion() == "0") return;
                m_manager.setVersion("0");
                m_filterDirty = true;
            });
            universalItem.setBack();

            // 动态项
            for (auto& version : result.versions) {
                auto& item = m_versionMenu.addRadio(version, brls::getStr("page/storeModList/versionSpecificDesc", version));
                item.setIcon(format::themedIconPath("img/menu/hVerson"));
                item.setSelected([this, version]{ return m_manager.getVersion() == version; });
                item.onSelected([this, version]{
                    if (m_manager.getVersion() == version) return;
                    m_manager.setVersion(version);
                    m_filterDirty = true;
                });
                item.setBack();
            }

            m_versionsLoaded = true;
        });
    }, token);
}
