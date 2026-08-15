/**
 * Home - 主页面
 */

#include "ui/page/home.hpp"
#include "common/config.hpp"
#include "common/settings.hpp"
#include "core/appUpdater.hpp"
#include "core/audio.hpp"
#include "core/device.hpp"
#include "core/frameQueue.hpp"
#include "core/modManager.hpp"
#include "core/storeGameIconCache.hpp"
#include "ui/dataSource/gameCardDS.hpp"
#include "ui/navigation/navigationGroups.hpp"
#include "ui/page/addGame.hpp"
#include "ui/page/help.hpp"
#include "ui/page/modList.hpp"
#include "ui/page/search.hpp"
#include "ui/page/storeGameList.hpp"
#include "ui/page/versionHistory.hpp"
#include "ui/view/dialog/customDialog.hpp"
#include "ui/view/dialog/longPressDialog.hpp"
#include "ui/view/dialog/progressDialog.hpp"
#include "ui/view/dialog/scrollDialog.hpp"
#include "ui/view/gameCard.hpp"
#include "ui/view/longTextBox.hpp"
#include "utils/format.hpp"
#include "utils/keyboard.hpp"
#include <algorithm>
#include <borealis/core/cache_helper.hpp>
#include <borealis/core/i18n.hpp>
#include <chrono>
#include <climits>
#include <cstdlib>
#include <switch.h>
#include <utility>
#include <vector>

Home::Home() {
    inflateFromXMLRes("xml/view/page/home.xml");

    setHeader();
}

void Home::setHeader() {
    HeaderState headerState;
    headerState.setNavigation(createMainNavigationState(MainNavigationPage::Home));
    ShellState::setHeaderState(headerState);
}

void Home::onContentAvailable() {
    startStartupUpdateCheck();

    // 如果为空提示找不到mod
    if (m_gameManager.games().empty()) showEmptyHint();

    setupGridPage();

    // - 键：打开搜索页面
    registerAction(brls::getStr("page/home/search"), brls::BUTTON_BACK, [this](...) {
        Audio::instance()->play(SoundEffect::Enter);
        std::vector<std::string> names;
        for (auto& game : m_gameManager.games())
            names.push_back(game.displayName);
        Page::pushPage(new Search(names, [this](int index) {
            m_grid->selectRowAt(index, false);
            m_grid->instantFocus(index);
        }));
        return true;
    });

    // + 键：收藏/取消收藏
    registerAction(brls::getStr("page/home/favorite"), brls::BUTTON_START, [this](...) {
        Audio::instance()->play(SoundEffect::Click);
        toggleFavorite();
        return true;
    });

    // X 键：切换排序方向（页面级操作，NACP 加载完成前禁用）
    registerAction(brls::getStr(m_gameManager.sortAsc() ? "page/home/sortAsc" : "page/home/sortDesc"), brls::BUTTON_Y, [this](...) {
        Audio::instance()->play(SoundEffect::Click);
        toggleSort();
        return true;
    });
    setNacpActionsAvailable(false);

    // ZR：新增游戏页面（隐藏 hint）
    registerAction("", brls::BUTTON_RT, [this](...) {
        Audio::instance()->play(SoundEffect::Enter);
        openAddGamePage();
        return true;
    }, true);

    // ZL：游戏商店页面（隐藏 hint）
    registerAction("", brls::BUTTON_LT, [this](...) {
        Audio::instance()->play(SoundEffect::Enter);
        Page::pushPage(new StoreGameList(m_gameManager), PageAnimType::SlideFromLeft);
        return true;
    }, true);

    setupMenu();
    startCardLoader();
    runStartupDialogs();
}

void Home::showEmptyHint() {
    ShellState::setIndexText("0 / 0");
    m_grid->setVisibility(brls::Visibility::GONE);
    m_noModHint->setVisibility(brls::Visibility::VISIBLE);
    brls::Application::giveFocus(m_noModHint);
    setNacpActionsAvailable(m_nacpComplete);
}

void Home::onResume() {
    // 清理 ModList 返回后留下的空项目
    std::string pendingCleanupPath = m_gameManager.consumePendingCleanup();
    if (!pendingCleanupPath.empty()) {
        brls::sync([this, pendingCleanupPath] {
            int cleanupIdx = m_gameManager.findByDirPath(pendingCleanupPath);
            int iconId = m_gameManager.games()[cleanupIdx].iconId;
            if (m_gameManager.cleanupGame(cleanupIdx) && iconId > 0) brls::TextureCache::instance().removeCache(iconId);
            if (m_gameManager.games().empty()) {
                showEmptyHint();
            } else {
                int newFocus = std::min(cleanupIdx, static_cast<int>(m_gameManager.games().size()) - 1);
                m_grid->deferReload(newFocus);
                m_focusedIndex = newFocus;
                setNacpActionsAvailable(m_nacpComplete);
            }
        });
        return;
    }

    // 页面恢复后刷新列表并聚焦目标项目
    std::string pendingFocusPath = m_gameManager.consumePendingFocusPath();
    if (pendingFocusPath.empty()) return;

    brls::sync([this, pendingFocusPath] {
        int newIdx = m_gameManager.findByDirPath(pendingFocusPath);
        if (m_grid->getVisibility() == brls::Visibility::GONE) {
            m_grid->setVisibility(brls::Visibility::VISIBLE);
            m_noModHint->setVisibility(brls::Visibility::GONE);
        }
        m_grid->deferReload(newIdx);
        m_focusedIndex = newIdx;
        setNacpActionsAvailable(m_nacpComplete);
    });
}

void Home::setupGridPage() {
    m_grid->setPadding(5, 15, 5, 40);
    m_grid->registerCell("GameCard", GameCard::create);

    auto* ds = new GameCardDS(m_gameManager.games(), [this](size_t index) { Page::pushPage(new ModList(index, m_gameManager)); }, [this](uint64_t appId) { launchGame(appId); });
    m_grid->setDataSource(ds);

    m_grid->setFocusChangeCallback([this](size_t index) {
        m_focusedIndex.store(index);
        ShellState::setIndexText(std::to_string(index + 1) + " / " + std::to_string(m_gameManager.games().size()));
    });
}

void Home::launchGame(uint64_t appId) {
    Result rc = appletRequestLaunchApplication(appId, nullptr);
    if (R_SUCCEEDED(rc)) return;

    CustomDialog::show(brls::getStr("page/home/launchFailed", format::resultHex(rc)), {
        {brls::getStr("page/home/ok"), [] { CustomDialog::close(); }}
    });
}

void Home::setNacpActionsAvailable(bool available) {
    bool enabled = available && !m_gameManager.games().empty();
    setActionAvailable(brls::BUTTON_BACK, enabled);
    setActionAvailable(brls::BUTTON_Y, enabled);
    setActionAvailable(brls::BUTTON_START, enabled);
}

void Home::toggleSort() {
    m_gameManager.toggleSortAsc();
    m_grid->setDefaultCellFocus(0);  // 11.8: 排序后回到顶部
    m_grid->reloadData();
    m_grid->instantFocus(0);
    updateActionHint(brls::BUTTON_Y, m_gameManager.sortAsc() ? brls::getStr("page/home/sortAsc") : brls::getStr("page/home/sortDesc"));
    brls::Application::getGlobalHintsUpdateEvent()->fire();
}

void Home::toggleFavorite() {
    if (m_gameManager.games().empty()) return;
    int idx = m_focusedIndex.load();
    auto& game = m_gameManager.games()[idx];
    uint64_t targetAppId = game.appId;

    m_gameManager.setFavorite(idx, !game.isFavorite);
    m_gameManager.sort();

    // 找到目标游戏排序后的新位置
    int newIdx = m_gameManager.findByAppId(targetAppId);

    m_grid->setDefaultCellFocus(newIdx);
    m_grid->reloadData();
    m_grid->instantFocus(newIdx);
}

void Home::applyGameDisplayName(int idx, const std::string& name) {
    m_gameManager.setDisplayName(idx, name);
    auto& game = m_gameManager.games()[idx];
    auto* cell = m_grid->getGridItemByIndex(idx);
    if (cell) static_cast<GameCard*>(cell)->setGame(game.displayName, game.version, game.modCount);
}

void Home::manualSetGameDisplayName() {
    int idx = m_focusedIndex.load();
    std::string name = keyboard::showText(brls::getStr("page/home/inputGameName"), brls::getStr("page/home/inputGameName"), m_gameManager.games()[idx].displayName, 64);
    if (name.empty()) return;
    applyGameDisplayName(idx, name);
}

std::any Home::fetchGameDisplayName(std::stop_token token) {
    int idx = m_focusedIndex.load();
    return m_gameManager.fetchDisplayName(idx, token);
}

void Home::checkFetchedGameDisplayName(std::any result) {
    auto r = std::any_cast<api::game::NameResult>(result);
    if (!r.success) {
        CustomDialog::show(r.error, {{brls::getStr("page/home/ok"), [] { CustomDialog::close(); }}});
        return;
    }

    auto onConfirm = [this, name = r.name] {
        applyGameDisplayName(m_focusedIndex.load(), name);
    };
    auto onEdit = [this, name = r.name] {
        std::string editedName = keyboard::showText(brls::getStr("page/home/confirmGameName"), brls::getStr("page/home/confirmGameName"), name, 64);
        if (editedName.empty()) return;
        applyGameDisplayName(m_focusedIndex.load(), editedName);
    };

    CustomDialog::show(brls::getStr("page/home/confirmNameMsg", r.name), {
        {brls::getStr("view/dialog/cancel"), [] { CustomDialog::close(); }},
        {brls::getStr("view/dialog/confirm"), [onConfirm] { CustomDialog::close(onConfirm); }},
        {brls::getStr("page/home/editBtn"), [onEdit] { CustomDialog::close(onEdit); }},
    });
}

void Home::checkFetchedVersionHistory(std::any result) {
    auto r = std::any_cast<api::app::VersionHistoryResult>(std::move(result));
    if (!r.success) {
        CustomDialog::show(r.error, {{brls::getStr("page/home/ok"), [] { CustomDialog::close(); }}});
        return;
    }

    if (r.list.empty()) {
        CustomDialog::show(brls::getStr("page/home/versionHistoryEmpty"), {{brls::getStr("page/home/ok"), [] { CustomDialog::close(); }}});
        return;
    }

    Page::pushPage(new VersionHistory(std::move(r.list)));
}

void Home::resetGameDisplayName() {
    int idx = m_focusedIndex.load();
    std::string restored = m_gameManager.getRestoredDisplayName(idx);
    if (restored.empty()) return;

    auto onConfirm = [this, idx, restored] {
        m_gameManager.deleteCustomDisplayName(idx, restored);
        auto& game = m_gameManager.games()[idx];
        auto* cell = m_grid->getGridItemByIndex(idx);
        if (cell) static_cast<GameCard*>(cell)->setGame(game.displayName, game.version, game.modCount);
    };

    CustomDialog::show(brls::getStr("page/home/restoreNameMsg", restored), {
        {brls::getStr("view/dialog/cancel"), [] { CustomDialog::close(); }},
        {brls::getStr("view/dialog/confirm"), [onConfirm] { CustomDialog::close(onConfirm); }},
    });
}

void Home::openAddGamePage() {
    if (m_nacpComplete) {
        Page::pushPage(new AddGame(m_gameManager), PageAnimType::SlideFromRight);
        return;
    }

    auto onCancel = [this]() {
        m_onNacpComplete = nullptr;
        CustomDialog::close();
    };

    CustomDialog::show(brls::getStr("page/home/loadingData"), {{brls::getStr("view/dialog/cancel"), onCancel}}, onCancel);

    m_onNacpComplete = [this]() {
        CustomDialog::close([this]() {
            Page::pushPage(new AddGame(m_gameManager), PageAnimType::SlideFromRight);
        });
    };
}

void Home::removeGame() {
    int idx = m_focusedIndex.load();
    auto& game = m_gameManager.games()[idx];

    if (ModManager::hasInstalledMod(game.dirPath)) {
        CustomDialog::show(brls::getStr("page/home/cannotRemove"), {{brls::getStr("page/home/ok"), [] { CustomDialog::close(); }}});
        return;
    }

    auto onConfirmDelete = [this, idx] {
        int iconId = m_gameManager.games()[idx].iconId;
        if (m_gameManager.removeGame(idx) && iconId > 0) brls::TextureCache::instance().removeCache(iconId);
        if (m_gameManager.games().empty()) {
            showEmptyHint();
        } else {
            int newFocus = std::min(idx, static_cast<int>(m_gameManager.games().size()) - 1);
            m_grid->deferReload(newFocus);
            m_focusedIndex = newFocus;
            setNacpActionsAvailable(m_nacpComplete);
        }
    };
    CustomDialog::show(brls::getStr("page/home/removeConfirm"), {
        {brls::getStr("view/dialog/cancel"), [] { CustomDialog::close(); }},
        {brls::getStr("view/dialog/cancel"), [] { CustomDialog::close(); }},
        {brls::getStr("page/home/removeBtn"), [onConfirmDelete] { CustomDialog::close(onConfirmDelete); }},
    });
}

void Home::deleteGame() {
    int idx = m_focusedIndex.load();
    auto& game = m_gameManager.games()[idx];

    if (ModManager::hasInstalledMod(game.dirPath)) {
        CustomDialog::show(brls::getStr("page/home/cannotDelete"), {{brls::getStr("page/home/ok"), [] { CustomDialog::close(); }}});
        return;
    }

    auto onConfirmDelete = [this, idx] {
        deviceControl::HomeButton::disable();
        ProgressDialog::show(brls::getStr("page/home/deletingGame"), {}, [] {});

        m_deleteGameTask = util::async([this, idx] {
            auto result = m_gameManager.deleteGameContents(idx, [](int deleted, int total, const char* fileName) {
                std::string fileNameStr = fileName ? fileName : "";
                bool scanning = (fileName == nullptr);
                brls::sync([=] {
                    if (scanning) {
                        ProgressDialog::setLeftText("");
                        ProgressDialog::setRightText("0 / " + std::to_string(total));
                        ProgressDialog::setMainProgress(0);
                    } else {
                        ProgressDialog::setLeftText(fileNameStr);
                        ProgressDialog::setRightText(std::to_string(deleted) + " / " + std::to_string(total));
                        ProgressDialog::setMainProgress(deleted * 100.0f / total);
                    }
                });
            });

            // 等待一段时间，避免因删除太快，导致进度框快速切换，视觉闪烁
            for (int i = 0; i < 30; i++) svcSleepThread(10000000ULL);  // 10ms × 30 = 300ms

            brls::sync([this, idx, result] {
                if (result.removeResult.status != fs::RemoveResult::Completed) {
                    m_gameManager.setModCount(idx, result.remainingModCount);
                    m_grid->reloadItem(idx);
                    deviceControl::HomeButton::enable();
                    CustomDialog::show(brls::getStr("page/home/deleteFailed", result.removeResult.errorPath, result.removeResult.deletedCount, result.removeResult.totalCount, result.removeResult.errorCode), {{brls::getStr("page/home/ok"), [] { CustomDialog::close(); }}});
                    return;
                }

                ProgressDialog::close([this, idx] {
                    int iconId = m_gameManager.games()[idx].iconId;
                    if (m_gameManager.deleteGame(idx) && iconId > 0) brls::TextureCache::instance().removeCache(iconId);
                    deviceControl::HomeButton::enable();

                    if (m_gameManager.games().empty()) {
                        showEmptyHint();
                    } else {
                        int newFocus = std::min(idx, static_cast<int>(m_gameManager.games().size()) - 1);
                        m_grid->deferReload(newFocus);
                        m_focusedIndex = newFocus;
                        setNacpActionsAvailable(m_nacpComplete);
                    }
                });
            });
        });
    };
    
    CustomDialog::show(brls::getStr("page/home/deleteConfirm"), {
        {brls::getStr("view/dialog/cancel"), [] { CustomDialog::close(); }},
        {brls::getStr("view/dialog/cancel"), [] { CustomDialog::close(); }},
        {brls::getStr("page/home/deleteBtn"), [onConfirmDelete] { CustomDialog::close(onConfirmDelete); }},
    });
}

void Home::clearTransit() {
    auto onConfirmClear = [this] {
        auto onCancel = [this] { m_clearTask.request_stop(); };
        ProgressDialog::show(brls::getStr("page/home/clearingTransit"), {{brls::getStr("view/dialog/cancel"), onCancel}}, onCancel);

        m_clearTask = util::async([this](std::stop_token token) {
            auto result = m_gameManager.clearTransit(token, [](int deleted, int total, const char* fileName) {
                std::string fileNameStr = fileName ? fileName : "";
                bool scanning = (fileName == nullptr);
                brls::sync([=] {
                    if (scanning) {
                        ProgressDialog::setRightText("0 / " + std::to_string(total));
                    } else {
                        ProgressDialog::setLeftText(fileNameStr);
                        ProgressDialog::setRightText(std::to_string(deleted) + " / " + std::to_string(total));
                        ProgressDialog::setMainProgress(deleted * 100.0f / total);
                    }
                });
            });

            // 等待一段时间，避免因清理的太快，导致进度框快速切换，视觉闪烁
            for (int i = 0; i < 30 && !token.stop_requested(); i++) svcSleepThread(10000000ULL);  // 10ms × 30 = 300ms

            // 任务完成，显示结果
            brls::sync([result] {
                std::string msg;
                switch (result.status) {
                    case fs::RemoveResult::Completed:
                        msg = brls::getStr("page/home/clearCompleted", result.elapsed);
                        break;
                    case fs::RemoveResult::Cancelled:
                        msg = brls::getStr("page/home/clearCancelled", result.deletedCount, result.totalCount);
                        break;
                    case fs::RemoveResult::FsError:
                        msg = brls::getStr("page/home/clearFailed", result.errorPath, result.deletedCount, result.totalCount, result.errorCode);
                        break;
                }
                CustomDialog::show(msg, {{brls::getStr("page/home/ok"), [] { CustomDialog::close(); }}});
            });
        });
    };

    CustomDialog::show(brls::getStr("page/home/clearConfirm"), {
        {brls::getStr("view/dialog/cancel"), [] { CustomDialog::close(); }},
        {brls::getStr("view/dialog/cancel"), [] { CustomDialog::close(); }},
        {brls::getStr("page/home/clearBtn"), onConfirmClear},
    });
}

void Home::deleteIconCache() {
    auto onConfirmDelete = [this] {
        deviceControl::HomeButton::disable();
        deviceControl::CpuBoost::enableFastLoad();
        CustomDialog::show(brls::getStr("page/home/deletingIconCache"), {}, [] {});

        m_deleteIconCacheTask = util::async([](std::stop_token) {
            StoreGameIconCache::deleteCache();
            brls::sync([] {
                deviceControl::CpuBoost::disable();
                deviceControl::HomeButton::enable();
                CustomDialog::show(brls::getStr("page/home/deleteIconCacheComplete"), {{brls::getStr("page/home/ok"), [] { CustomDialog::close(); }}});
            });
        });
    };

    CustomDialog::show(brls::getStr("page/home/deleteIconCacheConfirm"), {
        {brls::getStr("view/dialog/cancel"), [] { CustomDialog::close(); }},
        {brls::getStr("view/dialog/confirm"), onConfirmDelete},
    });
}

brls::Box* Home::createResetStateBox() {
    LongTextBoxConfig content;

    auto& reset = content.addEntry();
    reset.addTitle(brls::getStr("page/home/resetState"));
    reset.addBody(brls::getStr("page/home/resetStateBody"));

    auto& cases = content.addEntry();
    cases.addTitle(brls::getStr("page/home/resetStateCasesTitle"));
    cases.addBody(brls::getStr("page/home/resetStateCasesBody"), 1.3f);

    auto& warning = content.addEntry();
    warning.addTitle(brls::getStr("page/home/resetStateWarningTitle"));
    warning.addBody(brls::getStr("page/home/resetStateWarningBody"));

    return LongTextBox::create(content);
}

void Home::resetState() {
    auto onConfirm = [this] {
        deviceControl::HomeButton::disable();
        ProgressDialog::show(brls::getStr("page/home/resetting"), {}, nullptr);

        m_resetTask = util::async([this](std::stop_token) {
            m_gameManager.resetState([](int current, int total, const std::string& name) {
                brls::sync([=] {
                    ProgressDialog::setLeftText(name);
                    ProgressDialog::setRightText(std::to_string(current) + " / " + std::to_string(total));
                    if (total > 0) ProgressDialog::setMainProgress(current * 100.0f / total);
                });
            });

            for (int i = 0; i < 30; i++) svcSleepThread(10000000ULL);  // 300ms 防闪烁

            brls::sync([] {
                deviceControl::HomeButton::enable();
                CustomDialog::show(brls::getStr("page/home/resetComplete"), {{brls::getStr("page/home/ok"), [] { CustomDialog::close(); }}});
            });
        });
    };

    LongPressDialog::show(createResetStateBox(), brls::getStr("page/home/resetStateBtn"), 3.0f, onConfirm, [] { LongPressDialog::close(); });
}

void Home::setNickname() {
    std::string current = Settings::getString("modShop", "nickname");
    std::string name = keyboard::showText(brls::getStr("page/home/inputNickname"), brls::getStr("page/home/inputNickname"), current, 32);
    if (name.empty()) return;
    Settings::setString("modShop", "nickname", name);
}

void Home::setupGameManageMenu() {
    m_gameManageMenu.setIcon(format::themedIconPath("img/menu/manager"));

    auto& manualRenameItem = m_gameManageMenu.addAction(brls::getStr("page/home/manualRename"), brls::getStr("page/home/manualRenameDesc"));
    manualRenameItem.setIcon(format::themedIconPath("img/menu/manualInput"));
    manualRenameItem.setDisabled([this]{ return m_gameManager.games().empty(); });
    manualRenameItem.setBadge("\uE14A");
    manualRenameItem.onSelected([this]{ manualSetGameDisplayName(); });

    auto& onlineFetchNameItem = m_gameManageMenu.addTask<std::any>(brls::getStr("page/home/onlineFetch"), brls::getStr("page/home/onlineFetchDesc"));
    onlineFetchNameItem.setIcon(format::themedIconPath("img/menu/onlineFetch"));
    onlineFetchNameItem.setDisabled([this] { return m_gameManager.games().empty() || !deviceInfo::Network::isAvailable(); });
    onlineFetchNameItem.setBadge([] { return deviceInfo::Network::isAvailable() ? std::string("\uE14A") : brls::getStr("page/home/noNetwork"); });
    onlineFetchNameItem.setTask([this](std::stop_token token) { return fetchGameDisplayName(token); });
    onlineFetchNameItem.onComplete([this](std::any result) { checkFetchedGameDisplayName(result); });
    onlineFetchNameItem.setCancelable(true);

    auto& restoreNameItem = m_gameManageMenu.addAction(brls::getStr("page/home/restoreName"), brls::getStr("page/home/restoreNameDesc"));
    restoreNameItem.setIcon(format::themedIconPath("img/menu/restoreTitle"));
    restoreNameItem.setDisabled([this]{ return m_gameManager.games().empty(); });
    restoreNameItem.setBadge("\uE14A");
    restoreNameItem.onSelected([this]{ resetGameDisplayName(); });

    auto& removeGameItem = m_gameManageMenu.addAction(brls::getStr("page/home/removeGame"), brls::getStr("page/home/removeGameDesc"));
    removeGameItem.setIcon(format::themedIconPath("img/menu/removeItem"));
    removeGameItem.setDisabled([this]{ return m_gameManager.games().empty(); });
    removeGameItem.setBadge("\uE14A");
    removeGameItem.onSelected([this]{ removeGame(); });

    auto& deleteGameItem = m_gameManageMenu.addAction(brls::getStr("page/home/deleteGame"), brls::getStr("page/home/deleteGameDesc"));
    deleteGameItem.setIcon(format::themedIconPath("img/menu/clearTransferStation"));
    deleteGameItem.setDisabled([this]{ return m_gameManager.games().empty(); });
    deleteGameItem.setBadge("\uE14A");
    deleteGameItem.onSelected([this]{ deleteGame(); });

    auto& viewPathItem = m_gameManageMenu.addAction(brls::getStr("page/home/viewPath"), brls::getStr("page/home/viewPathDesc"));
    viewPathItem.setIcon(format::themedIconPath("img/menu/viewLocation"));
    viewPathItem.setDisabled([this]{ return m_gameManager.games().empty(); });
    viewPathItem.setBadge("\uE14A");
    viewPathItem.onSelected([this]{ CustomDialog::show(m_gameManager.games()[m_focusedIndex.load()].dirPath, {{brls::getStr("page/home/ok"), [] { CustomDialog::close(); }}}); });
}

void Home::setupSettingsMenu() {
    m_basicSettingsMenu.setIcon(format::themedIconPath("img/menu/setting"));
    m_advancedSettingsMenu.setIcon(format::themedIconPath("img/menu/otherSetting"));
    m_assistFeaturesMenu.setIcon(format::themedIconPath("img/menu/otherF"));

    auto& nicknameItem = m_basicSettingsMenu.addAction(brls::getStr("page/home/nickname"), brls::getStr("page/home/nicknameDesc"));
    nicknameItem.setIcon(format::themedIconPath("img/menu/userNanme"));
    nicknameItem.setBadge([]{ return Settings::getString("modShop", "nickname", brls::getStr("page/home/anonymousUser")); });
    nicknameItem.onSelected([this]{ setNickname(); });
    nicknameItem.setStayOpen();

    auto& langItem = m_basicSettingsMenu.addSubmenu(brls::getStr("page/home/language"), brls::getStr("page/home/languageDesc"));
    langItem.setIcon(format::themedIconPath("img/menu/language"));
    langItem.setBadge([]{
        std::string l = Settings::getString("UI", "language", "auto");
        if (l == "zh-Hans") return brls::getStr("page/home/langZhCNItem");
        if (l == "zh-Hant") return brls::getStr("page/home/langZhTWItem");
        if (l == "en-US")   return brls::getStr("page/home/langEnUSItem");
        if (l == "pt-BR")   return brls::getStr("page/home/langPtBRItem");
        if (l == "ja")      return brls::getStr("page/home/langJaItem");
        return brls::getStr("page/home/langAutoItem");
    });
    langItem.setPage(m_langMenu);

    auto& themeItem = m_basicSettingsMenu.addSubmenu(brls::getStr("page/home/themeColor"), brls::getStr("page/home/themeColorDesc"));
    themeItem.setIcon(format::themedIconPath("img/menu/theme"));
    themeItem.setBadge([]{
        std::string t = Settings::getString("UI", "theme", "auto");
        if (t == "light") return brls::getStr("page/home/themeLightItem");
        if (t == "dark")  return brls::getStr("page/home/themeDarkItem");
        return brls::getStr("page/home/themeAuto");
    });
    themeItem.setPage(m_themeMenu);

    auto& soundItem = m_basicSettingsMenu.addSwitch(brls::getStr("page/home/soundEffect"), brls::getStr("page/home/soundEffectDesc"));
    soundItem.setIcon(format::themedIconPath("img/menu/music"));
    soundItem.setState([]{
        return !Settings::getBool("Audio", "muted", false);
    });
    soundItem.setTask([](bool value) {
        Settings::setBool("Audio", "muted", !value);
        brls::sync([value] { Audio::instance()->setMuted(!value); });
    });

    auto& fpsItem = m_advancedSettingsMenu.addSwitch(brls::getStr("page/home/fpsMonitor"), brls::getStr("page/home/fpsMonitorDesc"));
    fpsItem.setIcon(format::themedIconPath("img/menu/fps"));
    fpsItem.setState([]{
        return Settings::getBool("UI", "showFps", false);
    });
    fpsItem.setTask([this](bool value) {
        Settings::setBool("UI", "showFps", value);
        brls::sync([this, value] { Page::setShowFps(value); });
    });

    auto& memItem = m_advancedSettingsMenu.addSwitch(brls::getStr("page/home/memMonitor"), brls::getStr("page/home/memMonitorDesc"));
    memItem.setIcon(format::themedIconPath("img/menu/ram"));
    memItem.setState([]{
        return Settings::getBool("UI", "showMem", false);
    });
    memItem.setTask([this](bool value) {
        Settings::setBool("UI", "showMem", value);
        brls::sync([this, value] { Page::setShowMem(value); });
    });

    auto& cpuBoostItem = m_advancedSettingsMenu.addSwitch(brls::getStr("page/home/cpuBoost"), brls::getStr("page/home/cpuBoostDesc"));
    cpuBoostItem.setIcon(format::themedIconPath("img/menu/CPU"));
    cpuBoostItem.setState([]{
        return Settings::getBool("Performance", "cpuBoost", true);
    });
    cpuBoostItem.setTask([](bool value) {
        Settings::setBool("Performance", "cpuBoost", value);
    });

    auto& transitItem = m_assistFeaturesMenu.addAction(brls::getStr("page/home/clearTransitItem"), brls::getStr("page/home/clearTransitDesc"));
    transitItem.setIcon(format::themedIconPath("img/menu/clearTransferStation"));
    transitItem.setBadge([this]() {
        int count = ModManager::transitModCount();
        return count > 0 ? std::to_string(count) + " MOD" : "0 MOD";
    });
    transitItem.onSelected([this]{ clearTransit(); });

    auto& iconCacheItem = m_assistFeaturesMenu.addAction(brls::getStr("page/home/deleteIconCacheItem"), brls::getStr("page/home/deleteIconCacheDesc"));
    iconCacheItem.setIcon(format::themedIconPath("img/menu/fixIcon"));
    iconCacheItem.setBadge("\uE14A");
    iconCacheItem.onSelected([this]{ deleteIconCache(); });

    auto& resetItem = m_assistFeaturesMenu.addAction(brls::getStr("page/home/resetState"), brls::getStr("page/home/resetStateBody"));
    resetItem.setIcon(format::themedIconPath("img/menu/restoreTitle"));
    resetItem.setDisabled([this]{ return !m_nacpComplete; });
    resetItem.setBadge("\uE14A");
    resetItem.onSelected([this]{ resetState(); });
}

void Home::setupLangMenu() {
    m_langMenu.setIcon(format::themedIconPath("img/menu/language"));

    auto currentLang = []{ return Settings::getString("UI", "language", "auto"); };
    auto applyLang = [](const std::string& value) {
        Settings::setString("UI", "language", value);
        CustomDialog::show(brls::getStr("page/home/langRestartMsg"), {
            {brls::getStr("view/dialog/cancel"), []{ CustomDialog::close(); }},
            {brls::getStr("page/home/restart"), []{
                CustomDialog::show(brls::getStr("page/home/restarting"), {}, []{});
                envSetNextLoad(config::getNroPath(), config::getNroPath());
                brls::Application::quit();
            }},
        });
    };

    auto& autoItem = m_langMenu.addRadio(brls::getStr("page/home/langAutoItem"), brls::getStr("page/home/langAutoDesc"));
    autoItem.setIcon(format::themedIconPath("img/menu/auto"));
    autoItem.setSelected([=]{ return currentLang() == "auto"; });
    autoItem.onSelected([=]{ applyLang("auto"); });
    autoItem.setStayOpen();

    auto& zhCNItem = m_langMenu.addRadio(brls::getStr("page/home/langZhCNItem"), brls::getStr("page/home/langZhCNDesc"));
    zhCNItem.setIcon(format::themedIconPath("img/menu/zh"));
    zhCNItem.setSelected([=]{ return currentLang() == "zh-Hans"; });
    zhCNItem.onSelected([=]{ applyLang("zh-Hans"); });
    zhCNItem.setStayOpen();

    auto& zhTWItem = m_langMenu.addRadio(brls::getStr("page/home/langZhTWItem"), brls::getStr("page/home/langZhTWDesc"));
    zhTWItem.setIcon(format::themedIconPath("img/menu/zhHant"));
    zhTWItem.setSelected([=]{ return currentLang() == "zh-Hant"; });
    zhTWItem.onSelected([=]{ applyLang("zh-Hant"); });
    zhTWItem.setStayOpen();

    auto& enUSItem = m_langMenu.addRadio(brls::getStr("page/home/langEnUSItem"), brls::getStr("page/home/langEnUSDesc"));
    enUSItem.setIcon(format::themedIconPath("img/menu/en"));
    enUSItem.setSelected([=]{ return currentLang() == "en-US"; });
    enUSItem.onSelected([=]{ applyLang("en-US"); });
    enUSItem.setStayOpen();

    auto& ptBRItem = m_langMenu.addRadio(brls::getStr("page/home/langPtBRItem"), brls::getStr("page/home/langPtBRDesc"));
    ptBRItem.setIcon(format::themedIconPath("img/menu/ptBr"));
    ptBRItem.setSelected([=]{ return currentLang() == "pt-BR"; });
    ptBRItem.onSelected([=]{ applyLang("pt-BR"); });
    ptBRItem.setStayOpen();

    auto& jaItem = m_langMenu.addRadio(brls::getStr("page/home/langJaItem"), brls::getStr("page/home/langJaDesc"));
    jaItem.setIcon(format::themedIconPath("img/menu/jp"));
    jaItem.setSelected([=]{ return currentLang() == "ja"; });
    jaItem.onSelected([=]{ applyLang("ja"); });
    jaItem.setStayOpen();
}

void Home::setupThemeMenu() {
    m_themeMenu.setIcon(format::themedIconPath("img/menu/theme"));

    auto currentTheme = []{ return Settings::getString("UI", "theme", "auto"); };
    auto applyTheme = [](const std::string& value) {
        Settings::setString("UI", "theme", value);
        CustomDialog::show(brls::getStr("page/home/themeRestartMsg"), {
            {brls::getStr("view/dialog/cancel"), []{ CustomDialog::close(); }},
            {brls::getStr("page/home/restart"), []{
                CustomDialog::show(brls::getStr("page/home/restarting"), {}, []{});
                envSetNextLoad(config::getNroPath(), config::getNroPath());
                brls::Application::quit();
            }},
        });
    };

    auto& themeAutoItem = m_themeMenu.addRadio(brls::getStr("page/home/themeAuto"), brls::getStr("page/home/themeAutoDesc"));
    themeAutoItem.setIcon(format::themedIconPath("img/menu/auto"));
    themeAutoItem.setSelected([=]{ return currentTheme() == "auto"; });
    themeAutoItem.onSelected([=]{ applyTheme("auto"); });
    themeAutoItem.setStayOpen();

    auto& themeLightItem = m_themeMenu.addRadio(brls::getStr("page/home/themeLightItem"), brls::getStr("page/home/themeLightDesc"));
    themeLightItem.setIcon(format::themedIconPath("img/menu/lightTheme"));
    themeLightItem.setSelected([=]{ return currentTheme() == "light"; });
    themeLightItem.onSelected([=]{ applyTheme("light"); });
    themeLightItem.setStayOpen();

    auto& themeDarkItem = m_themeMenu.addRadio(brls::getStr("page/home/themeDarkItem"), brls::getStr("page/home/themeDarkDesc"));
    themeDarkItem.setIcon(format::themedIconPath("img/menu/darkTheme"));
    themeDarkItem.setSelected([=]{ return currentTheme() == "dark"; });
    themeDarkItem.onSelected([=]{ applyTheme("dark"); });
    themeDarkItem.setStayOpen();
}

void Home::setupSortFilterMenu() {
    m_sortFilterMenu.setIcon(format::themedIconPath("img/menu/sortType"));

    auto applySortMode = [this](SortMode mode) {
        m_gameManager.setSortMode(mode);
        updateActionHint(brls::BUTTON_Y, m_gameManager.sortAsc() ? brls::getStr("page/home/sortAsc") : brls::getStr("page/home/sortDesc"));
        brls::Application::getGlobalHintsUpdateEvent()->fire();
    };

    auto& sortNameItem = m_sortFilterMenu.addRadio(brls::getStr("page/home/sortByName"), brls::getStr("page/home/sortByNameDesc"));
    sortNameItem.setIcon(format::themedIconPath("img/menu/title"));
    sortNameItem.setSelected([this]{ return m_gameManager.sortMode() == SortMode::Name; });
    sortNameItem.onSelected([applySortMode]{ applySortMode(SortMode::Name); });
    sortNameItem.setStayOpen();

    auto& sortCountItem = m_sortFilterMenu.addRadio(brls::getStr("page/home/sortByCount"), brls::getStr("page/home/sortByCountDesc"));
    sortCountItem.setIcon(format::themedIconPath("img/menu/modNumber"));
    sortCountItem.setSelected([this]{ return m_gameManager.sortMode() == SortMode::ModCount; });
    sortCountItem.onSelected([applySortMode]{ applySortMode(SortMode::ModCount); });
    sortCountItem.setStayOpen();

    auto& sortRecentItem = m_sortFilterMenu.addRadio(brls::getStr("page/home/sortByRecent"), brls::getStr("page/home/sortByRecentDesc"));
    sortRecentItem.setIcon(format::themedIconPath("img/menu/last"));
    sortRecentItem.setSelected([this]{ return m_gameManager.sortMode() == SortMode::RecentPlay; });
    sortRecentItem.onSelected([applySortMode]{ applySortMode(SortMode::RecentPlay); });
    sortRecentItem.setStayOpen();

    // TODO: 筛选功能（未完成）
}

void Home::setupMenu() {
    setupGameManageMenu();
    setupSortFilterMenu();
    setupThemeMenu();
    setupLangMenu();
    setupSettingsMenu();

    // ── 主菜单 ──
    m_menu.setIcon(format::themedIconPath("img/menu/menu"));

    m_menu.setShowFakeHighlight([this]{ return !m_gameManager.games().empty(); });

    auto& manageSubmenuItem = m_menu.addSubmenu(brls::getStr("page/home/manageSubmenu"), brls::getStr("page/home/manageSubmenuDesc"));
    manageSubmenuItem.setIcon(format::themedIconPath("img/menu/manager"));
    manageSubmenuItem.setBadge("\uE14A");
    manageSubmenuItem.setPage(m_gameManageMenu);

    auto& sortFilterItem = m_menu.addSubmenu(brls::getStr("page/home/sortFilter"), brls::getStr("page/home/sortFilterDesc"));
    sortFilterItem.setIcon(format::themedIconPath("img/menu/sortType"));
    sortFilterItem.setBadge("\uE14A");
    sortFilterItem.setPage(m_sortFilterMenu);
    sortFilterItem.setDisabled([this]{ return !m_nacpComplete || m_gameManager.games().empty(); });

    auto& basicSettingsItem = m_menu.addSubmenu(brls::getStr("page/home/basicSettings"), brls::getStr("page/home/basicSettingsDesc"));
    basicSettingsItem.setIcon(format::themedIconPath("img/menu/setting"));
    basicSettingsItem.setBadge("\uE14A");
    basicSettingsItem.setPage(m_basicSettingsMenu);

    auto& advancedSettingsItem = m_menu.addSubmenu(brls::getStr("page/home/advancedSettings"), brls::getStr("page/home/advancedSettingsDesc"));
    advancedSettingsItem.setIcon(format::themedIconPath("img/menu/otherSetting"));
    advancedSettingsItem.setBadge("\uE14A");
    advancedSettingsItem.setPage(m_advancedSettingsMenu);

    auto& assistFeaturesItem = m_menu.addSubmenu(brls::getStr("page/home/assistFeatures"), brls::getStr("page/home/assistFeaturesDesc"));
    assistFeaturesItem.setIcon(format::themedIconPath("img/menu/otherF"));
    assistFeaturesItem.setBadge("\uE14A");
    assistFeaturesItem.setPage(m_assistFeaturesMenu);

    auto& versionHistoryItem = m_menu.addTask<std::any>(brls::getStr("page/home/versionHistoryItem"), brls::getStr("page/home/versionHistoryItemDesc"));
    versionHistoryItem.setIcon(format::themedIconPath("img/menu/hVerson"));
    versionHistoryItem.setDisabled([] { return !deviceInfo::Network::isAvailable(); });
    versionHistoryItem.setBadge([] { return deviceInfo::Network::isAvailable() ? std::string("\uE14A") : brls::getStr("page/home/noNetwork"); });
    versionHistoryItem.setTask([](std::stop_token token) { return AppUpdater::instance().fetchVersionHistory(token); });
    versionHistoryItem.onComplete([this](std::any result) { checkFetchedVersionHistory(std::move(result)); });
    versionHistoryItem.setCancelable(true);

    auto& aboutItem = m_menu.addAction(brls::getStr("page/home/aboutItem"), brls::getStr("page/home/aboutItemDesc"));
    aboutItem.setIcon(format::themedIconPath("img/menu/about"));
    aboutItem.setBadge("\uE14A");
    aboutItem.onSelected([this]{ Page::pushPage(new Help()); });

    registerAction(brls::getStr("page/home/menu"), brls::BUTTON_X, [this](...) {
        Audio::instance()->play(SoundEffect::Enter);
        m_menu.show();
        return true;
    });
}

// Switch 启动时，Borealis 会在首次窗口尺寸稳定后触发窗口尺寸事件，
// TextureCache 会将该事件发生前创建的纹理缓存标记为失效。
// 如果立即加载第一张游戏卡片，其图标可能先写入缓存，随后被标记为失效，
// 导致其他页面无法再通过 App ID 查询该纹理。
// 因此这里等待首次窗口尺寸事件执行后，再开始依次加载游戏卡片。
// 订阅需要在使用后取消，避免以后插拔底座时再次启动卡片加载；
// 但不能在事件遍历过程中直接删除当前回调，所以通过 brls::sync()
// 延迟到下一次主线程任务中取消订阅。
void Home::startCardLoader() {
    auto* event = brls::Application::getWindowSizeChangedEvent();
    m_windowSizeChangedSubscription = event->subscribe([this, event] {
        submitNextCard();
        brls::sync([this, event] {
            event->unsubscribe(m_windowSizeChangedSubscription);
        });
    });
}

void Home::submitNextCard() {
    auto& games = m_gameManager.games();
    size_t gameIdx = games.size();
    int focusedIndex = m_focusedIndex.load();
    int bestDist = INT_MAX;
    for (size_t index = 0; index < games.size(); index++) {
        if (!games[index].isPending) continue;
        int dist = std::abs(static_cast<int>(index) - focusedIndex);
        if (dist < bestDist) {
            bestDist = dist;
            gameIdx = index;
        }
    }
    if (gameIdx == games.size()) {
        finishNacpLoading();
        return;
    }

    uint64_t appId = games[gameIdx].appId;
    m_nacpLoader = util::async([this, appId](std::stop_token token) {
        auto meta = m_gameManager.fetchMetadataByAppId(appId);
        auto image = imageDecoder::decodeJpeg(meta.icon.data(), meta.icon.size());
        std::string name = std::move(meta.name);
        std::string version = std::move(meta.version);

        if (token.stop_requested()) return;
        FrameQueue::enqueue(token, [this, appId, name = std::move(name), version = std::move(version), image = std::move(image)]() mutable {
            applyCard(appId, std::move(name), std::move(version), std::move(image));
        });
    });
}

int Home::loadGameIcon(uint64_t appId, const imageDecoder::DecodedImage& image) {
    auto& textureCache = brls::TextureCache::instance();
    std::string key = format::appIdHex(appId);
    int iconId = textureCache.getCache(key);
    if (iconId > 0) return iconId;
    if (image.width <= 0 || image.height <= 0 || image.pixels.empty()) return -1;

    iconId = nvgCreateImageRGBA(brls::Application::getNVGContext(), image.width, image.height, 0, image.pixels.data());
    if (iconId <= 0) return -1;

    textureCache.addCache(key, iconId);
    return iconId;
}

void Home::applyCard(uint64_t appId, std::string name, std::string version, imageDecoder::DecodedImage image) {
    auto indices = m_gameManager.findAllByAppId(appId);
    auto& games = m_gameManager.games();
    int iconId = -1;
    for (int idx : indices) {
        if (games[idx].iconId > 0) {
            iconId = games[idx].iconId;
            break;
        }
    }
    if (iconId <= 0) iconId = loadGameIcon(appId, image);

    for (int idx : indices) {
        if (!version.empty()) m_gameManager.setVersion(idx, version, false);
        if (!name.empty()) m_gameManager.setGameName(idx, name, false);
        auto& game = games[idx];
        game.iconId = iconId;
        game.isPending = false;
        m_grid->reloadItem(idx);
    }

    submitNextCard();
}

void Home::finishNacpLoading() {
    m_gameManager.saveJsonCache();
    m_nacpComplete = true;
    setNacpActionsAvailable(m_nacpComplete);
    if (m_onNacpComplete) {
        m_onNacpComplete();
        m_onNacpComplete = nullptr;
    }
}

void Home::startStartupUpdateCheck() {
    if (!deviceInfo::Network::isAvailable()) return;

    m_startupUpdateTask = util::async([this](std::stop_token token) {
        auto& updater = AppUpdater::instance();
        updater.check(token, APP_VERSION);
        if (token.stop_requested()) return;
        if (!updater.hasUpdate()) return;

        brls::sync([this, token] {
            if (token.stop_requested()) return;
            if (!m_allowForcedUpdate) return;

            if (!Page::isActive()) return;

            showForcedUpdateDialog();
        });
    });
}

brls::Box* Home::createUpdateDetailBox() {
    auto& updater = AppUpdater::instance();
    std::string releaseNotes = updater.releaseNotes();
    std::string updateTitle = brls::getStr("page/home/updateDialogTitle") + updater.tagName();
    std::string updateBody =
        brls::getStr("page/home/updateDialogPublished") + updater.publishTime() + "\n" +
        brls::getStr("page/home/updateDialogChangelogBelow");

    LongTextBoxConfig content;

    auto& update = content.addEntry();
    update.addTitle(updateTitle);
    update.addBody(updateBody);

    auto& support = content.addEntry();
    support.addTitle(brls::getStr("page/home/updateDialogSupport"));
    support.addBody(brls::getStr("page/home/updateDialogSupportDesc"));
    support.addQr(config::donateWechatQr, brls::getStr("page/home/updateDialogQrWechat"));
    support.addQr(config::donatePaypalQr, brls::getStr("page/home/updateDialogQrPaypal"));

    auto& community = content.addEntry();
    community.addTitle(brls::getStr("page/home/updateDialogCommunity"));
    community.addQr(config::communityQqQr, brls::getStr("page/home/updateDialogQrQQ"));
    community.addQr(config::communityDiscordQr, brls::getStr("page/home/updateDialogQrDiscord"));

    auto& changelog = content.addEntry();
    changelog.addTitle(brls::getStr("page/home/updateDialogChangelog"));
    changelog.addBody(releaseNotes.empty() ? brls::getStr("page/home/updateDialogNoChangelog") : releaseNotes, 1.3f);

    return LongTextBox::create(content);
}

void Home::showForcedUpdateDialog() {
    Audio::instance()->play(SoundEffect::Enter);
    ScrollDialog::show(createUpdateDetailBox(), brls::getStr("page/home/exit"), [] { brls::Application::quit(); }, brls::getStr("page/home/updateBtn"), [this] { startUpdateDownload(); }, [] { brls::Application::quit(); });
}

void Home::startUpdateDownload() {
    deviceControl::HomeButton::disable();
    ProgressDialog::show(brls::getStr("page/home/downloading"), {}, [] {});
    ProgressDialog::setLeftText(brls::getStr("page/home/calculating"));
    ProgressDialog::setRightText("--:--");

    m_updateTask = util::async([](std::stop_token token) {
        constexpr size_t minUpdateFileSize = 1024 * 1024;
        using Clock = std::chrono::steady_clock;
        auto lastProgressTime = Clock::now();
        auto lastBarTime = Clock::now();
        auto lastTextTime = Clock::now();
        long long lastBytes = 0;
        double smoothedSpeed = 0;

        auto onProgress = [&](size_t total, size_t now) -> bool {
            if (total < minUpdateFileSize) return true;

            auto t = Clock::now();
            bool textUpdate = std::chrono::duration<double>(t - lastTextTime).count() >= 1.0;
            bool barUpdate = std::chrono::duration<double>(t - lastBarTime).count() >= 0.1;

            if (textUpdate) {
                double dt = std::chrono::duration<double>(t - lastProgressTime).count();
                double instant = dt > 0.01 ? (now - lastBytes) / dt : 0;
                constexpr double alpha = 0.3;
                smoothedSpeed = (smoothedSpeed < 1.0) ? instant : alpha * instant + (1.0 - alpha) * smoothedSpeed;
                lastBytes = static_cast<long long>(now);
                lastProgressTime = t;
            }

            if (textUpdate || barUpdate) {
                double speed = smoothedSpeed;
                float pct = total > 0 ? now * 100.0f / total : 0;
                long long remaining = total > now ? total - now : 0;
                int etaSeconds = speed > 0 ? static_cast<int>(remaining / speed) : 0;
                size_t nowCopy = now;
                size_t totalCopy = total;

                brls::sync([=] {
                    if (barUpdate) ProgressDialog::setMainProgress(pct);
                    if (textUpdate) {
                        ProgressDialog::setLeftText(format::transferSpeed(speed) + "  " + format::fileSize(nowCopy) + " / " + format::fileSize(totalCopy));
                        ProgressDialog::setRightText(format::duration(etaSeconds, "HH:MM:SS"));
                    }
                });

                if (textUpdate) lastTextTime = t;
                if (barUpdate) lastBarTime = t;
            }
            return true;
        };

        auto result = AppUpdater::instance().download(token, onProgress);
        bool cancelled = token.stop_requested();

        brls::sync([result, cancelled] {
            deviceControl::HomeButton::enable();
            if (cancelled) {
                CustomDialog::close();
                return;
            }
            if (!result.success) {
                CustomDialog::show(result.error, {{brls::getStr("page/home/ok"), []{ CustomDialog::close(); }}});
                return;
            }
            CustomDialog::show(brls::getStr("page/home/restarting"), {}, []{});
            brls::sync([] {
                AppUpdater::instance().install();
                envSetNextLoad(config::getNroPath(), config::getNroPath());
                brls::Application::quit();
            });
        });
    });
}

void Home::showDuplicateWarning(int duplicateCount) {
    std::string msg = brls::getStr("page/home/duplicateWarning", duplicateCount);
    Audio::instance()->play(SoundEffect::Enter);
    CustomDialog::show(msg, {{brls::getStr("page/home/ok"), [] { CustomDialog::close(); }}});
}

brls::Box* Home::createFirstLaunchBox() {
    LongTextBoxConfig content;

    auto& notice = content.addEntry();
    notice.addTitle(brls::getStr("page/home/firstLaunchNoticeTitle"));
    notice.addBody(brls::getStr("page/home/firstLaunchNoticeBody"));

    auto& quickStart = content.addEntry();
    quickStart.addTitle(brls::getStr("page/home/firstLaunchQuickStartTitle"));
    quickStart.addBody(brls::getStr("page/home/firstLaunchQuickStartBody"));

    auto& disclaimer = content.addEntry();
    disclaimer.addTitle(brls::getStr("page/home/firstLaunchDisclaimerTitle"));
    disclaimer.addBody(brls::getStr("page/home/firstLaunchDisclaimerBody"));

    auto& support = content.addEntry();
    support.addTitle(brls::getStr("page/home/updateDialogSupport"));
    support.addBody(brls::getStr("page/home/updateDialogSupportDesc"));
    support.addQr(config::donateWechatQr, brls::getStr("page/home/updateDialogQrWechat"));
    support.addQr(config::donatePaypalQr, brls::getStr("page/home/updateDialogQrPaypal"));

    auto& community = content.addEntry();
    community.addTitle(brls::getStr("page/home/updateDialogCommunity"));
    community.addQr(config::communityQqQr, brls::getStr("page/home/updateDialogQrQQ"));
    community.addQr(config::communityDiscordQr, brls::getStr("page/home/updateDialogQrDiscord"));

    return LongTextBox::create(content);
}

void Home::runStartupDialogs() {

    // Home::onContentAvailable 处于 ShellActivity 的 pushActivity 执行中，
    // ShellActivity 尚未入栈，直接推送 Dialog 会导致栈序错乱，需延迟到下一帧执行

    bool isFirstLaunch = Settings::getBool("home", "firstLaunch", true);
    int duplicateCount = m_gameManager.duplicateCount();
    if (!isFirstLaunch && duplicateCount == 0) return;

    m_allowForcedUpdate = false;

    brls::sync([this, isFirstLaunch, duplicateCount]() {
        if (!isFirstLaunch) {
            showDuplicateWarning(duplicateCount);
            return;
        }

        auto onConfirm = [this, duplicateCount]() {
            brls::sync([this, duplicateCount]() {
                Settings::setBool("home", "firstLaunch", false);
                LongPressDialog::close([this, duplicateCount]() {
                    if (duplicateCount > 0) showDuplicateWarning(duplicateCount);
                });
            });
        };

        LongPressDialog::show(createFirstLaunchBox(), brls::getStr("page/home/firstLaunchBtn"), 5.0f, onConfirm);
    });
}
