/**
 * StoreModList - 商店模组列表页面实现
 */

#include "ui/page/storeModList.hpp"
#include "common/config.hpp"
#include "core/audio.hpp"
#include "ui/core/pageHost.hpp"
#include "ui/dataSource/storeModListDS.hpp"
#include "ui/page/storeGameList.hpp"
#include "ui/page/storeModDetail.hpp"
#include "ui/view/dialog/customDialog.hpp"
#include "ui/view/dialog/keyboardInput.hpp"
#include "ui/view/qrCodeView.hpp"
#include "ui/view/storeModCard.hpp"
#include "utils/format.hpp"
#include <borealis/core/i18n.hpp>
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

static const char* modTypeLabel(const std::string& modType) {
    for (auto& option : modTypeFilterOptions()) {
        if (option.value == modType) return option.label.c_str();
    }
    return modTypeFilterOptions().front().label.c_str();
}

static std::string versionLabel(const std::string& version) {
    if (version.empty()) return brls::getStr("page/storeModList/versionAll");
    if (version == "0") return brls::getStr("page/storeModList/versionUniversal");
    return version;
}

StoreModList::StoreModList(std::string gameTid, std::string gameName, GameManager& gameManager, ModManager* localModManager, std::string gameVersion, bool fromModList)
    : m_manager(std::move(gameTid)), m_gameName(std::move(gameName)), m_gameManager(gameManager), m_localModManager(localModManager), m_gameVersion(std::move(gameVersion)), m_fromModList(fromModList) {
    inflateFromXMLRes("xml/view/page/storeModList.xml");
}

StoreModList::~StoreModList() {
    m_stopSource.request_stop();
}

void StoreModList::onContentAvailable() {
    ShellState::setTitle(m_gameName);
    ShellState::setSubtitle(brls::getStr("page/storeModList/notInstalled"));
    if (m_gameVersion != "...") ShellState::setSubtitle(brls::getStr("page/storeModList/localVersion", format::cleanVersion(m_gameVersion)));
    m_emptyHint->setVisibility(brls::Visibility::VISIBLE);
    setupGrid();
    setupSearch();
    setupFilterMenu();
    setQueryActionsAvailable(false);
    loadVersionMenu();

    prepareLocalModManager();

    // B 键：有搜索词时重置搜索，否则返回
    registerAction("", brls::BUTTON_B, [this](...) {
        return handleBackOrResetSearch();
    }, true);

    // ZR：从 ModList 进入时，快捷返回
    if (isFromModList()) {
        registerAction("", brls::BUTTON_RT, [this](...) {
            Audio::instance()->play(SoundEffect::Enter);
            Page::popPage(PageAnimType::SlideFromRight);
            return true;
        }, true);
    }

    // + 键：上传模组
    registerAction(brls::getStr("page/storeModList/uploadMod"), brls::BUTTON_START, [](...) {
        Audio::instance()->play(SoundEffect::Enter);
        QrCodeView::show(config::websiteUrl, config::websiteUrl);
        return true;
    });

    loadNextPage();
}

void StoreModList::prepareLocalModManager() {
    if (m_localModManager) return;

    uint64_t appId = format::appIdFromHex(m_manager.gameTid());
    int idx = m_gameManager.findByAppId(appId);
    if (idx < 0) return;

    m_pageModManager.emplace(m_gameManager.games()[idx]);
    m_localModManager = &m_pageModManager.value();
}

bool StoreModList::isFromModList() const {
    return m_fromModList;
}

void StoreModList::setupGrid() {
    m_grid->setPadding(17, 15, 17, 40);
    m_grid->registerCell("StoreModCard", StoreModCard::create);
    m_grid->onNextPage([this] {
        if (!m_loading && m_manager.hasMore()) loadNextPage();
    });
    m_grid->setFocusChangeCallback([this](size_t index) {
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
    auto token = m_stopSource.get_token();

    ThreadPool::instance().submit([this, gameTid, page, sort, keyword, version, modType](std::stop_token token) {
        auto result = api::mod::fetchModList(gameTid, page, 20, sort, keyword, version, modType, token);
        if (token.stop_requested()) return;
        brls::sync([this, result = std::move(result), token]() mutable {
            if (token.stop_requested()) return;
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

    size_t oldSize = m_manager.storeModList().size();
    m_manager.appendPage(std::move(result));
    auto& storeMods = m_manager.storeModList();
    for (size_t i = oldSize; i < storeMods.size(); i++) {
        applyLocalState(storeMods[i]);
    }
    if (storeMods.empty()) {
        m_emptyHint->setText(brls::getStr("page/storeModList/noMods"));
        brls::Application::giveFocus(m_emptyHint);
        setQueryActionsAvailable(true);
        return;
    }
    m_emptyHint->setVisibility(brls::Visibility::GONE);

    if (!m_grid->getDataSource()) {
        // 首次：设置数据源 + 获取焦点
        m_grid->setDataSource(new StoreModListDS(storeMods, [this](size_t index) {
            openDetail(index);
        }));
        brls::Application::giveFocus(m_grid);
        setQueryActionsAvailable(true);
    } else {
        // 后续分页：只通知数据变化，保持滚动位置
        m_grid->notifyDataChanged();
        setQueryActionsAvailable(true);
    }
}

void StoreModList::applyLocalState(api::mod::ModList& mod) {
    mod.downloaded = false;
    mod.hasUpdate = false;
    mod.installed = false;

    if (!m_localModManager) return;

    int idx = m_localModManager->findByModID(mod.modId);
    if (idx < 0) return;

    mod.downloaded = true;

    auto& localMod = m_localModManager->mods()[idx];
    mod.installed = localMod.isInstalled;
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
    Page::pushPage(new StoreModDetail(mod.modId, m_manager.gameTid(), m_gameName, m_gameManager, onReturn, m_localModManager, m_gameVersion, m_fromModList));
}

void StoreModList::reloadData() {
    // 让旧异步回调失效（不阻塞）
    m_stopSource.request_stop();
    m_stopSource = std::stop_source{};

    // 重置数据
    m_manager.reset();
    m_grid->setDefaultCellFocus(0);
    m_grid->setDataSource(nullptr);
    m_emptyHint->setText(brls::getStr("page/storeModList/loadingHint"));
    m_emptyHint->setVisibility(brls::Visibility::VISIBLE);
    setQueryActionsAvailable(false);
    m_loading = false;

    // 焦点交给加载提示，避免悬空（旧 cell 已销毁）
    brls::Application::giveFocus(m_emptyHint);

    // 更新标题
    ShellState::setTitle(m_manager.getKeyword().empty() ? m_gameName : brls::getStr("page/storeModList/titleSearch", m_manager.getKeyword()));

    loadNextPage();
}

bool StoreModList::handleBackOrResetSearch() {
    if (!m_manager.getKeyword().empty()) {
        Audio::instance()->play(SoundEffect::Click);
        m_manager.setKeyword("");
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
            reloadData();
        }, brls::getStr("page/storeModList/searchPlaceholder"), 50);
        return true;
    });
}

void StoreModList::setupFilterMenu() {
    // ── 排序子菜单 ──
    m_sortMenu.title = brls::getStr("page/storeModList/sortMenuTitle");
    for (auto& option : sortOptions()) {
        auto& item = m_sortMenu.addItem(option.label, option.hint);
        item.setBadge([this, value = option.value]{
            return m_manager.getSort() == value ? std::string("\uE14B") : std::string();
        });
        item.setAction([this, value = option.value]{
            m_manager.setSort(value);
            m_filterDirty = true;
        });
        item.setPopPage();
    }

    // ── 模组类型子菜单 ──
    m_modTypeMenu.title = brls::getStr("page/storeModList/typeMenuTitle");
    for (auto& option : modTypeFilterOptions()) {
        auto& item = m_modTypeMenu.addItem(option.label, option.hint);
        item.setBadge([this, value = option.value]{
            return m_manager.getModType() == value ? std::string("\uE14B") : std::string();
        });
        item.setAction([this, value = option.value]{
            m_manager.setModType(value);
            m_filterDirty = true;
        });
        item.setPopPage();
    }

    // ── 游戏版本子菜单（由 loadVersionMenu 异步构建） ──
    m_versionMenu.title = brls::getStr("page/storeModList/versionMenuTitle");

    // ── 筛选主菜单 ──
    m_filterMenu.title = brls::getStr("page/storeModList/filterMenuTitle");
    m_filterMenu.shouldShowFakeHighlight = [this]{ return !m_manager.storeModList().empty(); };
    m_filterMenu.onDismiss = [this]{
        if (m_filterDirty) {
            m_filterDirty = false;
            reloadData();
        }
    };

    auto& sortEntry = m_filterMenu.addItem(brls::getStr("page/storeModList/sortEntry"), brls::getStr("page/storeModList/sortEntryDesc"));
    sortEntry.setBadge([this]{ return sortLabel(m_manager.getSort()); });
    sortEntry.setSubmenu(&m_sortMenu);

    auto& typeEntry = m_filterMenu.addItem(brls::getStr("page/storeModList/typeEntry"), brls::getStr("page/storeModList/typeEntryDesc"));
    typeEntry.setBadge([this]{ return modTypeLabel(m_manager.getModType()); });
    typeEntry.setSubmenu(&m_modTypeMenu);

    auto& versionEntry = m_filterMenu.addItem(brls::getStr("page/storeModList/versionEntry"), brls::getStr("page/storeModList/versionEntryDesc"));
    versionEntry.setBadge([this]{ return versionLabel(m_manager.getVersion()); });
    versionEntry.setDisabled([this]{ return !m_versionsLoaded; });
    versionEntry.setSubmenu(&m_versionMenu);

    auto& resetItem = m_filterMenu.addItem(brls::getStr("page/storeModList/resetFilter"), brls::getStr("page/storeModList/resetFilterDesc"));
    resetItem.setDisabled([this]{
        return m_manager.getSort() == "latest" && m_manager.getModType().empty() && m_manager.getVersion().empty();
    });
    resetItem.setAction([this]{
        m_manager.resetFilter();
        m_filterDirty = false;
        reloadData();
    });

    registerAction(brls::getStr("page/storeModList/filterAction"), brls::BUTTON_X, [this](...) {
        Audio::instance()->play(SoundEffect::Enter);
        m_filterMenu.show();
        return true;
    });
}

void StoreModList::loadVersionMenu() {
    auto gameTid = m_manager.gameTid();
    auto token = m_stopSource.get_token();
    ThreadPool::instance().submit([this, gameTid](std::stop_token token) {
        auto result = api::mod::fetchModGameVersions(gameTid, token);
        if (token.stop_requested()) return;
        brls::sync([this, result = std::move(result), token]() mutable {
            if (token.stop_requested()) return;
            if (!result.success) return;

            // 静态项
            auto& allItem = m_versionMenu.addItem(brls::getStr("page/storeModList/versionAll"), brls::getStr("page/storeModList/versionAllDesc"));
            allItem.setBadge([this]{ return m_manager.getVersion().empty() ? std::string("\uE14B") : std::string(); });
            allItem.setAction([this]{
                m_manager.setVersion("");
                m_filterDirty = true;
            });
            allItem.setPopPage();

            auto& universalItem = m_versionMenu.addItem(brls::getStr("page/storeModList/versionUniversal"), brls::getStr("page/storeModList/versionUniversalDesc"));
            universalItem.setBadge([this]{ return m_manager.getVersion() == "0" ? std::string("\uE14B") : std::string(); });
            universalItem.setAction([this]{
                m_manager.setVersion("0");
                m_filterDirty = true;
            });
            universalItem.setPopPage();

            // 动态项
            for (auto& version : result.versions) {
                auto& item = m_versionMenu.addItem(version, brls::getStr("page/storeModList/versionSpecificDesc", version));
                item.setBadge([this, version]{ return m_manager.getVersion() == version ? std::string("\uE14B") : std::string(); });
                item.setAction([this, version]{
                    m_manager.setVersion(version);
                    m_filterDirty = true;
                });
                item.setPopPage();
            }

            m_versionsLoaded = true;
        });
    }, token);
}
