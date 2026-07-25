/**
 * Home - 主页面
 */

#include "ui/page/home.hpp"
#include "common/config.hpp"
#include "common/settings.hpp"
#include "core/appUpdater.hpp"
#include "core/audio.hpp"
#include "core/device.hpp"
#include "core/modManager.hpp"
#include "core/storeGameIconCache.hpp"
#include "ui/dataSource/gameCardDS.hpp"
#include "ui/page/addGame.hpp"
#include "ui/page/help.hpp"
#include "ui/page/modList.hpp"
#include "ui/page/search.hpp"
#include "ui/page/storeGameList.hpp"
#include "ui/page/storeModList.hpp"
#include "ui/view/dialog/customDialog.hpp"
#include "ui/view/dialog/longPressDialog.hpp"
#include "ui/view/dialog/progressDialog.hpp"
#include "ui/view/dialog/scrollDialog.hpp"
#include "ui/view/gameCard.hpp"
#include "ui/view/longTextBox.hpp"
#include "ui/view/qrCodeView.hpp"
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

    ShellState::setTitle(brls::getStr("page/home/pageTitle"));
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
    startNacpLoader();
    runStartupDialogs();
}

void Home::showEmptyHint() {
    m_grid->setVisibility(brls::Visibility::GONE);
    m_noModHint->setVisibility(brls::Visibility::VISIBLE);
    brls::Application::giveFocus(m_noModHint);
    setNacpActionsAvailable(m_nacpComplete);
}

void Home::onResume() {
    // 延迟到页面返回完成后执行，防止焦点恢复流程覆盖目标焦点
    uint64_t removeAppId = m_gameManager.consumePendingRemove();
    if (removeAppId != 0) {
        brls::sync([this, removeAppId] {
            int removeIdx = m_gameManager.findByAppId(removeAppId);
            m_gameManager.removeGame(removeIdx);
            if (m_gameManager.games().empty()) {
                showEmptyHint();
            } else {
                int newFocus = std::min(removeIdx, static_cast<int>(m_gameManager.games().size()) - 1);
                m_grid->deferReload(newFocus);
                m_focusedIndex = newFocus;
                setNacpActionsAvailable(m_nacpComplete);
            }
        });
        return;
    }

    uint64_t appId = m_gameManager.consumePendingFocus();
    if (appId == 0) return;
    brls::sync([this, appId] {
        int newIdx = m_gameManager.findByAppId(appId);
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
    m_grid->setPadding(17, 15, 17, 40);
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
        m_gameManager.removeGame(idx);
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

void Home::setupGameNameMenu() {
    m_gameNameMenu.title = brls::getStr("page/home/gameNameMenuTitle");

    auto& manualRenameItem = m_gameNameMenu.addItem(brls::getStr("page/home/manualRename"), brls::getStr("page/home/manualRenameDesc"));
    manualRenameItem.setDisabled([this]{ return m_gameManager.games().empty(); });
    manualRenameItem.setAction([this]{ manualSetGameDisplayName(); });

    auto& onlineFetchNameItem = m_gameNameMenu.addItem(brls::getStr("page/home/onlineFetch"), brls::getStr("page/home/onlineFetchDesc"));
    onlineFetchNameItem.setDisabled([this] { return m_gameManager.games().empty() || !deviceInfo::Network::isAvailable(); });
    onlineFetchNameItem.setBadge([] { return deviceInfo::Network::isAvailable() ? "" : brls::getStr("page/home/noNetwork"); });
    onlineFetchNameItem.setAsyncAction([this](std::stop_token token) -> std::any { return fetchGameDisplayName(token); });
    onlineFetchNameItem.setAction([this](std::any result) { checkFetchedGameDisplayName(result); });

    auto& restoreNameItem = m_gameNameMenu.addItem(brls::getStr("page/home/restoreName"), brls::getStr("page/home/restoreNameDesc"));
    restoreNameItem.setDisabled([this]{ return m_gameManager.games().empty(); });
    restoreNameItem.setBadge([this]() {
        if (m_gameManager.games().empty()) return std::string();
        return m_gameManager.getRestoredDisplayName(m_focusedIndex.load());
    });
    restoreNameItem.setAction([this]{ resetGameDisplayName(); });
}

void Home::setupGameManageMenu() {
    m_gameManageMenu.title = brls::getStr("page/home/gameManageMenuTitle");

    auto& renameItem = m_gameManageMenu.addItem(brls::getStr("page/home/renameSubmenu"), brls::getStr("page/home/renameSubmenuDesc"));
    renameItem.setDisabled([this]{ return m_gameManager.games().empty(); });
    renameItem.setBadge("\uE14A");
    renameItem.setSubmenu(&m_gameNameMenu);

    auto& removeGameItem = m_gameManageMenu.addItem(brls::getStr("page/home/removeGame"), brls::getStr("page/home/removeGameDesc"));
    removeGameItem.setDisabled([this]{ return m_gameManager.games().empty(); });
    removeGameItem.setAction([this]{ removeGame(); });

    auto& viewPathItem = m_gameManageMenu.addItem(brls::getStr("page/home/viewPath"), brls::getStr("page/home/viewPathDesc"));
    viewPathItem.setDisabled([this]{ return m_gameManager.games().empty(); });
    viewPathItem.setBadge([this]() {
        if (m_gameManager.games().empty()) return std::string();
        return m_gameManager.getDirName(m_focusedIndex.load());
    });
    viewPathItem.setAction([this]{ CustomDialog::show(m_gameManager.games()[m_focusedIndex.load()].dirPath, {{brls::getStr("page/home/ok"), [] { CustomDialog::close(); }}}); });
}

void Home::setupSettingsMenu() {
    m_settingsMenu.title = brls::getStr("page/home/settingsMenuTitle");
    m_basicSettingsMenu.title = brls::getStr("page/home/basicSettings");
    m_advancedSettingsMenu.title = brls::getStr("page/home/advancedSettings");
    m_assistFeaturesMenu.title = brls::getStr("page/home/assistFeatures");

    auto& nicknameItem = m_settingsMenu.addItem(brls::getStr("page/home/nickname"), brls::getStr("page/home/nicknameDesc"));
    nicknameItem.setBadge([]{ return Settings::getString("modShop", "nickname", brls::getStr("page/home/anonymousUser")); });
    nicknameItem.setAction([this]{ setNickname(); });
    nicknameItem.setStayOpen();

    auto& basicItem = m_settingsMenu.addItem(brls::getStr("page/home/basicSettings"), brls::getStr("page/home/basicSettingsDesc"));
    basicItem.setBadge("\uE14A");
    basicItem.setSubmenu(&m_basicSettingsMenu);

    auto& advancedItem = m_settingsMenu.addItem(brls::getStr("page/home/advancedSettings"), brls::getStr("page/home/advancedSettingsDesc"));
    advancedItem.setBadge("\uE14A");
    advancedItem.setSubmenu(&m_advancedSettingsMenu);

    auto& assistItem = m_settingsMenu.addItem(brls::getStr("page/home/assistFeatures"), brls::getStr("page/home/assistFeaturesDesc"));
    assistItem.setBadge("\uE14A");
    assistItem.setSubmenu(&m_assistFeaturesMenu);

    auto& langItem = m_basicSettingsMenu.addItem(brls::getStr("page/home/language"), brls::getStr("page/home/languageDesc"));
    langItem.setBadge([]{
        std::string l = Settings::getString("UI", "language", "auto");
        if (l == "zh-Hans") return brls::getStr("page/home/langZhCNItem");
        if (l == "zh-Hant") return brls::getStr("page/home/langZhTWItem");
        if (l == "en-US")   return brls::getStr("page/home/langEnUSItem");
        if (l == "pt-BR")   return brls::getStr("page/home/langPtBRItem");
        return brls::getStr("page/home/langAutoItem");
    });
    langItem.setSubmenu(&m_langMenu);

    auto& themeItem = m_basicSettingsMenu.addItem(brls::getStr("page/home/themeColor"), brls::getStr("page/home/themeColorDesc"));
    themeItem.setBadge([]{
        std::string t = Settings::getString("UI", "theme", "auto");
        if (t == "light") return brls::getStr("page/home/themeLightItem");
        if (t == "dark")  return brls::getStr("page/home/themeDarkItem");
        return brls::getStr("page/home/themeAuto");
    });
    themeItem.setSubmenu(&m_themeMenu);

    auto& soundItem = m_basicSettingsMenu.addItem(brls::getStr("page/home/soundEffect"), brls::getStr("page/home/soundEffectDesc"));
    soundItem.setBadge([]{ return Settings::getBool("Audio", "muted", false) ? brls::getStr("page/home/off") : brls::getStr("page/home/on"); });
    soundItem.setBadgeHighlight([]{ return !Settings::getBool("Audio", "muted", false); });
    soundItem.setStayOpen();
    soundItem.setAction([]{
        bool newVal = !Settings::getBool("Audio", "muted", false);
        Settings::setBool("Audio", "muted", newVal);
        Audio::instance()->setMuted(newVal);
    });

    auto& fpsItem = m_advancedSettingsMenu.addItem(brls::getStr("page/home/fpsMonitor"), brls::getStr("page/home/fpsMonitorDesc"));
    fpsItem.setBadge([]{ return Settings::getBool("UI", "showFps", false) ? brls::getStr("page/home/on") : brls::getStr("page/home/off"); });
    fpsItem.setBadgeHighlight([]{ return Settings::getBool("UI", "showFps", false); });
    fpsItem.setStayOpen();
    fpsItem.setAction([this]{
        bool newVal = !Settings::getBool("UI", "showFps", false);
        Settings::setBool("UI", "showFps", newVal);
        Page::setShowFps(newVal);
    });

    auto& memItem = m_advancedSettingsMenu.addItem(brls::getStr("page/home/memMonitor"), brls::getStr("page/home/memMonitorDesc"));
    memItem.setBadge([]{ return Settings::getBool("UI", "showMem", false) ? brls::getStr("page/home/on") : brls::getStr("page/home/off"); });
    memItem.setBadgeHighlight([]{ return Settings::getBool("UI", "showMem", false); });
    memItem.setStayOpen();
    memItem.setAction([this] {
        bool newVal = !Settings::getBool("UI", "showMem", false);
        Settings::setBool("UI", "showMem", newVal);
        Page::setShowMem(newVal);
    });

    auto& cpuBoostItem = m_advancedSettingsMenu.addItem(brls::getStr("page/home/cpuBoost"), brls::getStr("page/home/cpuBoostDesc"));
    cpuBoostItem.setBadge([]{ return Settings::getBool("Performance", "cpuBoost", true) ? brls::getStr("page/home/on") : brls::getStr("page/home/off"); });
    cpuBoostItem.setBadgeHighlight([]{ return Settings::getBool("Performance", "cpuBoost", true); });
    cpuBoostItem.setStayOpen();
    cpuBoostItem.setAction([]{
        bool newVal = !Settings::getBool("Performance", "cpuBoost", true);
        Settings::setBool("Performance", "cpuBoost", newVal);
    });

    auto& transitItem = m_assistFeaturesMenu.addItem(brls::getStr("page/home/clearTransitItem"), brls::getStr("page/home/clearTransitDesc"));
    transitItem.setBadge([this]() {
        int count = ModManager::transitModCount();
        return count > 0 ? std::to_string(count) + " MOD" : "0 MOD";
    });
    transitItem.setAction([this]{ clearTransit(); });

    auto& iconCacheItem = m_assistFeaturesMenu.addItem(brls::getStr("page/home/deleteIconCacheItem"), brls::getStr("page/home/deleteIconCacheDesc"));
    iconCacheItem.setAction([this]{ deleteIconCache(); });

    auto& resetItem = m_assistFeaturesMenu.addItem(brls::getStr("page/home/resetState"), brls::getStr("page/home/resetStateBody"));
    resetItem.setAction([this]{ resetState(); });
}

void Home::setupLangMenu() {
    m_langMenu.title = brls::getStr("page/home/langMenuTitle");

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

    auto& autoItem = m_langMenu.addItem(brls::getStr("page/home/langAutoItem"), brls::getStr("page/home/langAutoDesc"));
    autoItem.setBadge([=]{ return currentLang() == "auto" ? "\uE14B" : ""; });
    autoItem.setStayOpen();
    autoItem.setAction([=]{ applyLang("auto"); });

    auto& zhCNItem = m_langMenu.addItem(brls::getStr("page/home/langZhCNItem"), brls::getStr("page/home/langZhCNDesc"));
    zhCNItem.setBadge([=]{ return currentLang() == "zh-Hans" ? "\uE14B" : ""; });
    zhCNItem.setStayOpen();
    zhCNItem.setAction([=]{ applyLang("zh-Hans"); });

    auto& zhTWItem = m_langMenu.addItem(brls::getStr("page/home/langZhTWItem"), brls::getStr("page/home/langZhTWDesc"));
    zhTWItem.setBadge([=]{ return currentLang() == "zh-Hant" ? "\uE14B" : ""; });
    zhTWItem.setStayOpen();
    zhTWItem.setAction([=]{ applyLang("zh-Hant"); });

    auto& enUSItem = m_langMenu.addItem(brls::getStr("page/home/langEnUSItem"), brls::getStr("page/home/langEnUSDesc"));
    enUSItem.setBadge([=]{ return currentLang() == "en-US" ? "\uE14B" : ""; });
    enUSItem.setStayOpen();
    enUSItem.setAction([=]{ applyLang("en-US"); });

    auto& ptBRItem = m_langMenu.addItem(brls::getStr("page/home/langPtBRItem"), brls::getStr("page/home/langPtBRDesc"));
    ptBRItem.setBadge([=]{ return currentLang() == "pt-BR" ? "\uE14B" : ""; });
    ptBRItem.setStayOpen();
    ptBRItem.setAction([=]{ applyLang("pt-BR"); });
}

void Home::setupThemeMenu() {
    m_themeMenu.title = brls::getStr("page/home/themeMenuTitle");

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

    auto& themeAutoItem = m_themeMenu.addItem(brls::getStr("page/home/themeAuto"), brls::getStr("page/home/themeAutoDesc"));
    themeAutoItem.setBadge([=]{ return currentTheme() == "auto" ? "\uE14B" : ""; });
    themeAutoItem.setStayOpen();
    themeAutoItem.setAction([=]{ applyTheme("auto"); });

    auto& themeLightItem = m_themeMenu.addItem(brls::getStr("page/home/themeLightItem"), brls::getStr("page/home/themeLightDesc"));
    themeLightItem.setBadge([=]{ return currentTheme() == "light" ? "\uE14B" : ""; });
    themeLightItem.setStayOpen();
    themeLightItem.setAction([=]{ applyTheme("light"); });

    auto& themeDarkItem = m_themeMenu.addItem(brls::getStr("page/home/themeDarkItem"), brls::getStr("page/home/themeDarkDesc"));
    themeDarkItem.setBadge([=]{ return currentTheme() == "dark" ? "\uE14B" : ""; });
    themeDarkItem.setStayOpen();
    themeDarkItem.setAction([=]{ applyTheme("dark"); });
}

void Home::setupAddProjectMenu() {
    m_addProjectMenu.title = brls::getStr("page/home/addProjectSubmenu");

    auto& storeAddItem = m_addProjectMenu.addItem(brls::getStr("page/home/storeAddSubmenu"), brls::getStr("page/home/storeAddSubmenuDesc"));
    storeAddItem.setBadge("\uE14A");
    storeAddItem.setSubmenu(&m_storeAddMenu);

    auto& localAddItem = m_addProjectMenu.addItem(brls::getStr("page/home/localAdd"), brls::getStr("page/home/addGameDesc"));
    localAddItem.setBadge("\uE14A");
    localAddItem.setAction([this]{ openAddGamePage(); });
}

void Home::setupStoreAddMenu() {
    m_storeAddMenu.title = brls::getStr("page/home/storeAddSubmenu");
    m_storeAddMenu.shouldShowFakeHighlight = [this]{ return !m_gameManager.games().empty(); };

    auto& gameStoreItem = m_storeAddMenu.addItem(brls::getStr("page/home/gameStore"), brls::getStr("page/home/gameStoreDesc"));
    gameStoreItem.setBadge("\uE14A");
    gameStoreItem.setAction([this] {
        Page::pushPage(new StoreGameList(m_gameManager), PageAnimType::SlideFromLeft);
    });

    auto& modStoreItem = m_storeAddMenu.addItem(brls::getStr("page/home/modStore"), brls::getStr("page/home/modStoreDesc"));
    modStoreItem.setDisabled([this] { return m_gameManager.games().empty(); });
    modStoreItem.setBadge("\uE14A");
    modStoreItem.setAction([this] {
        auto& game = m_gameManager.games()[m_focusedIndex.load()];
        std::string tid = format::appIdHex(game.appId);
        Page::pushPage(new StoreModList(tid, game.displayName, m_gameManager, nullptr, game.version), PageAnimType::SlideFromLeft);
    });

    auto& uploadItem = m_storeAddMenu.addItem(brls::getStr("page/home/uploadMod"), brls::getStr("page/home/uploadModDesc"));
    uploadItem.setBadge("\uE14A");
    uploadItem.setAction([] {
        QrCodeView::show(config::websiteUrl);
    });
    uploadItem.setStayOpen();
}

void Home::setupSortFilterMenu() {
    m_sortFilterMenu.title = brls::getStr("page/home/sortFilterTitle");

    auto applySortMode = [this](SortMode mode) {
        m_gameManager.setSortMode(mode);
        m_grid->setDefaultCellFocus(0);
        m_grid->reloadData();
        m_grid->instantFocus(0);
        updateActionHint(brls::BUTTON_Y, m_gameManager.sortAsc() ? brls::getStr("page/home/sortAsc") : brls::getStr("page/home/sortDesc"));
        brls::Application::getGlobalHintsUpdateEvent()->fire();
    };

    auto& sortNameItem = m_sortFilterMenu.addItem(brls::getStr("page/home/sortByName"), brls::getStr("page/home/sortByNameDesc"));
    sortNameItem.setBadge([this]{ return m_gameManager.sortMode() == SortMode::Name ? "\uE14B" : ""; });
    sortNameItem.setAction([applySortMode]{ applySortMode(SortMode::Name); });

    auto& sortCountItem = m_sortFilterMenu.addItem(brls::getStr("page/home/sortByCount"), brls::getStr("page/home/sortByCountDesc"));
    sortCountItem.setBadge([this]{ return m_gameManager.sortMode() == SortMode::ModCount ? "\uE14B" : ""; });
    sortCountItem.setAction([applySortMode]{ applySortMode(SortMode::ModCount); });

    auto& sortRecentItem = m_sortFilterMenu.addItem(brls::getStr("page/home/sortByRecent"), brls::getStr("page/home/sortByRecentDesc"));
    sortRecentItem.setBadge([this]{ return m_gameManager.sortMode() == SortMode::RecentPlay ? "\uE14B" : ""; });
    sortRecentItem.setAction([applySortMode]{ applySortMode(SortMode::RecentPlay); });

    // TODO: 筛选功能（未完成）
}

void Home::setupMenu() {
    setupStoreAddMenu();
    setupAddProjectMenu();
    setupGameNameMenu();
    setupGameManageMenu();
    setupSortFilterMenu();
    setupThemeMenu();
    setupLangMenu();
    setupSettingsMenu();

    // ── 主菜单 ──
    m_menu.title = brls::getStr("page/home/mainMenuTitle");

    m_menu.shouldShowFakeHighlight = [this]{ return !m_gameManager.games().empty(); };

    auto& addProjectItem = m_menu.addItem(brls::getStr("page/home/addProjectSubmenu"), brls::getStr("page/home/addProjectSubmenuDesc"));
    addProjectItem.setBadge("\uE14A");
    addProjectItem.setSubmenu(&m_addProjectMenu);

    auto& manageSubmenuItem = m_menu.addItem(brls::getStr("page/home/manageSubmenu"), brls::getStr("page/home/manageSubmenuDesc"));
    manageSubmenuItem.setBadge("\uE14A");
    manageSubmenuItem.setSubmenu(&m_gameManageMenu);

    auto& sortFilterItem = m_menu.addItem(brls::getStr("page/home/sortFilter"), brls::getStr("page/home/sortFilterDesc"));
    sortFilterItem.setBadge("\uE14A");
    sortFilterItem.setSubmenu(&m_sortFilterMenu);
    sortFilterItem.setDisabled([this]{ return !m_nacpComplete || m_gameManager.games().empty(); });

    auto& settingsItem = m_menu.addItem(brls::getStr("page/home/settingsSubmenu"), brls::getStr("page/home/settingsSubmenuDesc"));
    settingsItem.setBadge("\uE14A");
    settingsItem.setSubmenu(&m_settingsMenu);

    auto& aboutItem = m_menu.addItem(brls::getStr("page/home/aboutItem"), brls::getStr("page/home/aboutItemDesc"));
    aboutItem.setBadge("\uE14A");
    aboutItem.setAction([this]{
        Page::pushPage(new Help());
    });

    auto& updateItem = m_menu.addItem(brls::getStr("page/home/checkUpdate"), brls::getStr("page/home/checkUpdateDesc"));
    updateItem.setDisabled([]{ return !deviceInfo::Network::isAvailable(); });
    updateItem.setBadge([]() -> std::string {
        if (!deviceInfo::Network::isAvailable()) return brls::getStr("page/home/noNetwork");
        return AppUpdater::instance().hasUpdate() ? brls::getStr("page/home/updateFound", AppUpdater::instance().tagName()) : "\uE14A";
    });
    updateItem.setAsyncAction([this](std::stop_token token) -> std::any { return checkForUpdate(token); });
    updateItem.setAction([this](std::any result) { showManualUpdateResult(result); });

    registerAction(brls::getStr("page/home/menu"), brls::BUTTON_X, [this](...) {
        Audio::instance()->play(SoundEffect::Enter);
        m_menu.show();
        return true;
    });
}

void Home::startNacpLoader() {
    m_nacpLoader = util::async([this](std::stop_token token) {
        // 构建任务列表
        std::vector<int> tasks;
        for (int i = 0; i < static_cast<int>(m_gameManager.games().size()); i++) {
            tasks.push_back(i);
        }

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
            tasks.erase(tasks.begin() + bestIdx);

            // 调 API 拿数据（后台线程）
            auto meta = m_gameManager.fetchMetadata(gameIdx);
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
                m_gameManager.saveJsonCache();
                m_nacpComplete = true;
                setNacpActionsAvailable(m_nacpComplete);
                if (m_onNacpComplete) {
                    m_onNacpComplete();
                    m_onNacpComplete = nullptr;
                }
            });
        }
    });
}

void Home::applyMetadata(int gameIdx, const GameMetadata& meta) {
    auto& game = m_gameManager.games()[gameIdx];

    // 更新 version
    if (!meta.version.empty())
        m_gameManager.setVersion(gameIdx, meta.version, false);

    // 更新官方名(gameName)，仅在无自定义名时同步到显示名
    if (!meta.name.empty())
        m_gameManager.setGameName(gameIdx, meta.name, false);

    // 创建 NVG 纹理（主线程安全），缓存交给框架管理
    if (!meta.icon.empty()) {
        NVGcontext* vg = brls::Application::getNVGContext();
        int iconId = nvgCreateImageMem(vg, 0, const_cast<unsigned char*>(meta.icon.data()), meta.icon.size());
        if (iconId > 0) {
            game.iconId = iconId;
            auto& tc = brls::TextureCache::instance();
            std::string key = m_gameManager.getAppIdKey(gameIdx);
            if (tc.getCache(key) == 0) tc.addCache(key, iconId);
        }
    }

    // 刷新可见 Cell（不可见的下次 cellForRow 自然绑定）
    auto* cell = m_grid->getGridItemByIndex(gameIdx);
    if (cell) {
        auto* card = static_cast<GameCard*>(cell);
        card->setGame(game.displayName, game.version, game.modCount);
        if (game.iconId > 0) card->setIcon(game.iconId);
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

std::any Home::checkForUpdate(std::stop_token token) {
    m_startupUpdateTask.request_stop();
    brls::sync([this] { m_allowForcedUpdate = false; });

    auto& updater = AppUpdater::instance();
    updater.check(token, APP_VERSION);
    return updater.hasUpdate();
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

void Home::showManualUpdateResult(std::any result) {
    if (!AppUpdater::instance().hasUpdate()) {
        CustomDialog::show(brls::getStr("page/home/upToDate"), {{brls::getStr("page/home/ok"), []{ CustomDialog::close(); }}});
        return;
    }

    ScrollDialog::show(createUpdateDetailBox(), brls::getStr("view/dialog/cancel"), [] { ScrollDialog::close(); }, brls::getStr("page/home/updateBtn"), [this] { startUpdateDownload(); }, [] { ScrollDialog::close(); });
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
