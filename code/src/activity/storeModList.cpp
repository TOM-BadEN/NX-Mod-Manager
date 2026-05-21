/**
 * StoreModList - 商店模组列表页面实现
 */

#include "activity/storeModList.hpp"
#include <borealis/core/i18n.hpp>
#include "dataSource/storeModListDS.hpp"
#include "view/storeModCard.hpp"
#include "view/customDialog.hpp"
#include "core/audio.hpp"
#include "activity/storeModDetail.hpp"
#include "utils/pageNav.hpp"
#include "view/keyboardInput.hpp"
#include "activity/storeGameList.hpp"
#include "utils/format.hpp"

// ── 筛选选项定义（展示层数据） ──

struct FilterOption {
    std::string value;  // API 值
    std::string label;  // 显示名
    std::string hint;   // 菜单提示（可为空）
};

static const std::vector<FilterOption>& sortOptions() {
    static const std::vector<FilterOption> options = {
        {"latest",   brls::getStr("storeModList/sortLatest"), brls::getStr("storeModList/sortLatestDesc")},
        {"download", brls::getStr("storeModList/sortDownload"), brls::getStr("storeModList/sortDownloadDesc")},
        {"like",     brls::getStr("storeModList/sortLike"), brls::getStr("storeModList/sortLikeDesc")},
    };
    return options;
}

static const std::vector<FilterOption>& modTypeFilterOptions() {
    static const std::vector<FilterOption> options = [] {
        std::vector<FilterOption> v;
        v.push_back({"", brls::getStr("storeModList/typeAll"), brls::getStr("storeModList/typeAllDesc")});
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
    if (version.empty()) return brls::getStr("storeModList/versionAll");
    if (version == "0")  return brls::getStr("storeModList/versionUniversal");
    return version;
}

StoreModList::StoreModList(std::string gameTid, std::string gameName, GameManager& gameManager, ModManager* localModManager, std::string gameVersion)
    : m_manager(std::move(gameTid)), m_gameName(std::move(gameName)), m_gameManager(gameManager), m_localModManager(localModManager), m_gameVersion(std::move(gameVersion)) {}

StoreModList::~StoreModList() {
    m_stopSource.request_stop();
}

void StoreModList::onContentAvailable() {
    m_frame->setTitle(m_gameName);
    if (m_gameVersion != "...") m_frame->setSubtitle(brls::getStr("storeModList/localVersion", format::cleanVersion(m_gameVersion)));
    m_emptyHint->setVisibility(brls::Visibility::VISIBLE);
    setupGrid();
    setupSearch();
    setupFilterMenu();
    setQueryActionsAvailable(false);
    loadVersionMenu();
    
    if (m_localModManager) m_localState.loadFromModManager(*m_localModManager);
    else m_localState.loadFromGame(m_gameManager, m_manager.gameTid());

    // B 键：返回
    m_frame->registerAction(brls::getStr("views/myframe/back"), brls::BUTTON_B, [this](...) {
        Audio::instance()->play(SoundEffect::Enter);
        auto stack = brls::Application::getActivitiesStack();
        auto* under = stack[stack.size() - 2];
        if (dynamic_cast<StoreGameList*>(under)) pageNav::pop(pageNav::Anim::NONE);
        else pageNav::pop(pageNav::Anim::SLIDE_RIGHT);
        return true;
    });

    // ZR：从 ModList 进入时，快捷返回
    if (m_localModManager) {
        m_frame->registerAction("", brls::BUTTON_RT, [this](...) {
            Audio::instance()->play(SoundEffect::Enter);
            pageNav::pop(pageNav::Anim::SLIDE_RIGHT);
            return true;
        }, true);
    }

    loadNextPage();
}

void StoreModList::setupGrid() {
    m_grid->setPadding(17, 15, 17, 40);
    m_grid->registerCell("StoreModCard", StoreModCard::create);
    m_grid->onNextPage([this] {
        if (!m_loading && m_manager.hasMore()) {
            loadNextPage();
        }
    });
    m_grid->setFocusChangeCallback([this](size_t index) {
        m_frame->setIndexText(std::to_string(index + 1) + " / " + std::to_string(m_manager.total()));
    });
}

void StoreModList::setQueryActionsAvailable(bool available) {
    m_frame->setActionAvailable(brls::BUTTON_BACK, available);
    m_frame->setActionAvailable(brls::BUTTON_X, available);
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
                brls::sync([token] {
                    if (token.stop_requested()) return;
                    pageNav::pop(pageNav::Anim::SLIDE_RIGHT);
                });
            }
        };
        CustomDialog::show(result.error, {{brls::getStr("storeModList/ok"), goBack}}, goBack);
        return;
    }

    m_manager.appendPage(std::move(result));
    for (auto& mod : m_manager.storeModList()) {
        mod.downloaded = m_localState.isDownloaded(mod.modId);
    }
    if (m_manager.storeModList().empty()) {
        m_emptyHint->setText(brls::getStr("storeModList/noMods"));
        brls::Application::giveFocus(m_emptyHint);
        setQueryActionsAvailable(true);
        return;
    }
    m_emptyHint->setVisibility(brls::Visibility::GONE);

    if (!m_grid->getDataSource()) {
        // 首次：设置数据源 + 获取焦点
        m_grid->setDataSource(new StoreModListDS(m_manager.storeModList(), [this](size_t index) {
            auto& mod = m_manager.storeModList()[index];
            auto onReturn = [this, index](int likes, int dislikes, int downloads, bool downloaded) {
                auto& mod = m_manager.storeModList()[index];
                mod.likeCount = likes;
                mod.dislikeCount = dislikes;
                mod.downloadCount = downloads;
                mod.downloaded = downloaded;
                if (downloaded) m_localState.markDownloaded(mod.modId);
                auto* cell = m_grid->getGridItemByIndex(index);
                if (cell) static_cast<StoreModCard*>(cell)->setMod(mod.modName, mod.uploadTime, mod.modType,
                    mod.gameVersion, likes, dislikes, downloads, downloaded);
            };
            pageNav::push(new StoreModDetail(mod.modId, m_manager.gameTid(), m_gameName, m_gameManager, onReturn, m_localModManager, m_gameVersion, m_localState.isDownloaded(mod.modId)));
        }));
        brls::Application::giveFocus(m_grid);
        setQueryActionsAvailable(true);
    } else {
        // 后续分页：只通知数据变化，保持滚动位置
        m_grid->notifyDataChanged();
        setQueryActionsAvailable(true);
    }
}

void StoreModList::reloadData() {
    // 让旧异步回调失效（不阻塞）
    m_stopSource.request_stop();
    m_stopSource = std::stop_source{};

    // 重置数据
    m_manager.reset();
    m_grid->setDefaultCellFocus(0);
    m_grid->setDataSource(nullptr);
    m_emptyHint->setText(brls::getStr("storeModList/loadingHint"));
    m_emptyHint->setVisibility(brls::Visibility::VISIBLE);
    setQueryActionsAvailable(false);
    m_loading = false;

    // 焦点交给加载提示，避免悬空（旧 cell 已销毁）
    brls::Application::giveFocus(m_emptyHint);

    // 更新标题
    m_frame->setTitle(m_manager.getKeyword().empty() ? m_gameName : brls::getStr("storeModList/titleSearch", m_manager.getKeyword()));

    loadNextPage();
}

void StoreModList::setupSearch() {
    m_frame->registerAction(brls::getStr("storeModList/searchAction"), brls::BUTTON_BACK, [this](...) {
        Audio::instance()->play(SoundEffect::Enter);
        KeyboardInput::show([this](std::string result) {
            if (result == m_manager.getKeyword()) return;
            m_manager.setKeyword(result);
            m_frame->setActionAvailable(brls::BUTTON_Y, !m_manager.getKeyword().empty());
            reloadData();
        }, brls::getStr("storeModList/searchPlaceholder"), 50);
        return true;
    });

    m_frame->registerAction(brls::getStr("storeModList/resetSearch"), brls::BUTTON_Y, [this](...) {
        Audio::instance()->play(SoundEffect::Click);
        m_manager.setKeyword("");
        m_frame->setActionAvailable(brls::BUTTON_Y, false);
        reloadData();
        return true;
    });
    m_frame->setActionAvailable(brls::BUTTON_Y, false);
}

void StoreModList::setupFilterMenu() {
    // ── 排序子菜单 ──
    m_sortMenu.title = brls::getStr("storeModList/sortMenuTitle");
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
    m_modTypeMenu.title = brls::getStr("storeModList/typeMenuTitle");
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
    m_versionMenu.title = brls::getStr("storeModList/versionMenuTitle");

    // ── 筛选主菜单 ──
    m_filterMenu.title = brls::getStr("storeModList/filterMenuTitle");
    m_filterMenu.shouldShowFakeHighlight = [this]{ return !m_manager.storeModList().empty(); };
    m_filterMenu.onDismiss = [this]{
        if (m_filterDirty) {
            m_filterDirty = false;
            reloadData();
        }
    };

    auto& sortEntry = m_filterMenu.addItem(brls::getStr("storeModList/sortEntry"), brls::getStr("storeModList/sortEntryDesc"));
    sortEntry.setBadge([this]{ return sortLabel(m_manager.getSort()); });
    sortEntry.setSubmenu(&m_sortMenu);

    auto& typeEntry = m_filterMenu.addItem(brls::getStr("storeModList/typeEntry"), brls::getStr("storeModList/typeEntryDesc"));
    typeEntry.setBadge([this]{ return modTypeLabel(m_manager.getModType()); });
    typeEntry.setSubmenu(&m_modTypeMenu);

    auto& versionEntry = m_filterMenu.addItem(brls::getStr("storeModList/versionEntry"), brls::getStr("storeModList/versionEntryDesc"));
    versionEntry.setBadge([this]{ return versionLabel(m_manager.getVersion()); });
    versionEntry.setDisabled([this]{ return !m_versionsLoaded; });
    versionEntry.setSubmenu(&m_versionMenu);

    auto& resetItem = m_filterMenu.addItem(brls::getStr("storeModList/resetFilter"), brls::getStr("storeModList/resetFilterDesc"));
    resetItem.setDisabled([this]{
        return m_manager.getSort() == "latest" && m_manager.getModType().empty() && m_manager.getVersion().empty();
    });
    resetItem.setAction([this]{
        m_manager.resetFilter();
        m_filterDirty = false;
        reloadData();
    });

    m_frame->registerAction(brls::getStr("storeModList/filterAction"), brls::BUTTON_X, [this](...) {
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
            auto& allItem = m_versionMenu.addItem(brls::getStr("storeModList/versionAll"), brls::getStr("storeModList/versionAllDesc"));
            allItem.setBadge([this]{ return m_manager.getVersion().empty() ? std::string("\uE14B") : std::string(); });
            allItem.setAction([this]{
                m_manager.setVersion("");
                m_filterDirty = true;
            });
            allItem.setPopPage();

            auto& universalItem = m_versionMenu.addItem(brls::getStr("storeModList/versionUniversal"), brls::getStr("storeModList/versionUniversalDesc"));
            universalItem.setBadge([this]{ return m_manager.getVersion() == "0" ? std::string("\uE14B") : std::string(); });
            universalItem.setAction([this]{
                m_manager.setVersion("0");
                m_filterDirty = true;
            });
            universalItem.setPopPage();

            // 动态项
            for (auto& version : result.versions) {
                auto& item = m_versionMenu.addItem(version, brls::getStr("storeModList/versionSpecificDesc", version));
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
