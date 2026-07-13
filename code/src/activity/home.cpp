/**
 * 主页面实现文件
 */

#include "activity/home.hpp"
#include <borealis/core/i18n.hpp>
#include "activity/modList.hpp"
#include "activity/searchActivity.hpp"
#include "activity/addGameActivity.hpp"
#include "view/gameCard.hpp"
#include "dataSource/gameCardDS.hpp"
#include "view/customDialog.hpp"
#include "view/progressDialog.hpp"
#include "view/scrollDialog.hpp"
#include "view/longTextBox.hpp"
#include "view/longPressDialog.hpp"
#include "utils/keyboard.hpp"
#include "core/device.hpp"
#include "core/audio.hpp"
#include "core/modManager.hpp"
#include "core/storeGameIconCache.hpp"
#include "activity/storeGameList.hpp"
#include "activity/help.hpp"
#include "activity/storeModList.hpp"
#include "utils/format.hpp"
#include "common/settings.hpp"
#include "common/config.hpp"
#include "core/appUpdater.hpp"
#include "utils/pageNav.hpp"
#include "view/qrCodeView.hpp"
#include <borealis/core/cache_helper.hpp>
#include <switch.h>

void Home::onContentAvailable() {
    startStartupUpdateCheck();

    // 如果为空提示找不到mod
    if (m_gameManager.games().empty()) showEmptyHint();

    setupGridPage();

    // - 键：打开搜索页面
    m_frame->registerAction(brls::getStr("home/search"), brls::BUTTON_BACK, [this](...) {
        Audio::instance()->play(SoundEffect::Enter);
        std::vector<std::string> names;
        for (auto& game : m_gameManager.games())
            names.push_back(game.displayName);
        pageNav::push(new SearchActivity(names, [this](int index) {
            m_grid->selectRowAt(index, false);
            m_grid->instantFocus(index);
        }));
        return true;
    });

    // + 键：收藏/取消收藏
    m_frame->registerAction(brls::getStr("home/favorite"), brls::BUTTON_START, [this](...) {
        Audio::instance()->play(SoundEffect::Click);
        toggleFavorite();
        return true;
    });

    // X 键：切换排序方向（页面级操作，NACP 加载完成前禁用）
    m_frame->registerAction(brls::getStr(m_gameManager.sortAsc() ? "home/sortAsc" : "home/sortDesc"), brls::BUTTON_Y, [this](...) {
        Audio::instance()->play(SoundEffect::Click);
        toggleSort();
        return true;
    });
    setNacpActionsAvailable(false);

    // ZR：新增游戏页面（隐藏 hint）
    m_frame->registerAction("", brls::BUTTON_RT, [this](...) {
        Audio::instance()->play(SoundEffect::Enter);
        openAddGamePage();
        return true;
    }, true);

    // ZL：游戏商店页面（隐藏 hint）
    m_frame->registerAction("", brls::BUTTON_LT, [this](...) {
        Audio::instance()->play(SoundEffect::Enter);
        pageNav::push(new StoreGameList(m_gameManager), pageNav::Anim::SLIDE_LEFT);
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
    // 延迟到 popActivity 完成后执行，防止焦点被 focusStack 恢复覆盖
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

    auto* ds = new GameCardDS(m_gameManager.games(), [this](size_t index) {
        auto& game = m_gameManager.games()[index];
        pageNav::push(new ModList(index, m_gameManager));
    });
    m_grid->setDataSource(ds);

    m_grid->setFocusChangeCallback([this](size_t index) {
        m_focusedIndex.store(index);
        m_frame->setIndexText(std::to_string(index + 1) + " / " + std::to_string(m_gameManager.games().size()));
    });
}

void Home::setNacpActionsAvailable(bool available) {
    bool enabled = available && !m_gameManager.games().empty();
    m_frame->setActionAvailable(brls::BUTTON_BACK, enabled);
    m_frame->setActionAvailable(brls::BUTTON_Y, enabled);
    m_frame->setActionAvailable(brls::BUTTON_START, enabled);
}

void Home::toggleSort() {
    m_gameManager.toggleSortAsc();
    m_grid->setDefaultCellFocus(0);  // 11.8: 排序后回到顶部
    m_grid->reloadData();
    m_grid->instantFocus(0);
    m_frame->updateActionHint(brls::BUTTON_Y, m_gameManager.sortAsc() ? brls::getStr("home/sortAsc") : brls::getStr("home/sortDesc"));
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
    std::string name = keyboard::showText(brls::getStr("home/inputGameName"), brls::getStr("home/inputGameName"), m_gameManager.games()[idx].displayName, 64);
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
        CustomDialog::show(r.error, {{brls::getStr("home/ok"), [] { CustomDialog::close(); }}});
        return;
    }

    auto onConfirm = [this, name = r.name] {
        applyGameDisplayName(m_focusedIndex.load(), name);
    };
    auto onEdit = [this, name = r.name] {
        std::string editedName = keyboard::showText(brls::getStr("home/confirmGameName"), brls::getStr("home/confirmGameName"), name, 64);
        if (editedName.empty()) return;
        applyGameDisplayName(m_focusedIndex.load(), editedName);
    };

    CustomDialog::show(brls::getStr("home/confirmNameMsg", r.name), {
        {brls::getStr("home/cancel"), [] { CustomDialog::close(); }},
        {brls::getStr("home/confirm"), [onConfirm] { CustomDialog::close(onConfirm); }},
        {brls::getStr("home/editBtn"), [onEdit] { CustomDialog::close(onEdit); }},
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
        if (cell) static_cast<GameCard*>(cell)->setGame(
            game.displayName, game.version, game.modCount);
    };

    CustomDialog::show(brls::getStr("home/restoreNameMsg", restored), {
        {brls::getStr("home/cancel"), [] { CustomDialog::close(); }},
        {brls::getStr("home/confirm"), [onConfirm] { CustomDialog::close(onConfirm); }},
    });
}

void Home::openAddGamePage() {
    if (m_nacpComplete) {
        pageNav::push(new AddGameActivity(m_gameManager), pageNav::Anim::SLIDE_RIGHT);
        return;
    }

    auto onCancel = [this]() {
        m_onNacpComplete = nullptr;
        CustomDialog::close();
    };

    CustomDialog::show(brls::getStr("home/loadingData"), {{brls::getStr("home/cancel"), onCancel}}, onCancel);

    m_onNacpComplete = [this]() {
        CustomDialog::close([this]() {
            pageNav::push(new AddGameActivity(m_gameManager), pageNav::Anim::SLIDE_RIGHT);
        });
    };
}

void Home::removeGame() {
    int idx = m_focusedIndex.load();
    auto& game = m_gameManager.games()[idx];

    if (ModManager::hasInstalledMod(game.dirPath)) {
        CustomDialog::show(brls::getStr("home/cannotRemove"), {{brls::getStr("home/ok"), [] { CustomDialog::close(); }}});
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
    CustomDialog::show(brls::getStr("home/removeConfirm"), {
        {brls::getStr("home/cancel"), [] { CustomDialog::close(); }},
        {brls::getStr("home/cancel"), [] { CustomDialog::close(); }},
        {brls::getStr("home/removeBtn"), [onConfirmDelete] { CustomDialog::close(onConfirmDelete); }},
    });
}

void Home::clearTransit() {
    auto onConfirmClear = [this] {
        auto onCancel = [this] { m_clearTask.request_stop(); };
        ProgressDialog::show(brls::getStr("home/clearingTransit"), {{brls::getStr("home/cancel"), onCancel}}, onCancel);

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
                        msg = brls::getStr("home/clearCompleted", result.elapsed);
                        break;
                    case fs::RemoveResult::Cancelled:
                        msg = brls::getStr("home/clearCancelled", result.deletedCount, result.totalCount);
                        break;
                    case fs::RemoveResult::FsError:
                        msg = brls::getStr("home/clearFailed", result.errorPath, result.deletedCount, result.totalCount, result.errorCode);
                        break;
                }
                CustomDialog::show(msg, {{brls::getStr("home/ok"), [] { CustomDialog::close(); }}});
            });
        });
    };

    CustomDialog::show(brls::getStr("home/clearConfirm"), {
        {brls::getStr("home/cancel"), [] { CustomDialog::close(); }},
        {brls::getStr("home/cancel"), [] { CustomDialog::close(); }},
        {brls::getStr("home/clearBtn"), onConfirmClear},
    });
}

void Home::deleteIconCache() {
    auto onConfirmDelete = [this] {
        deviceControl::HomeButton::disable();
        deviceControl::CpuBoost::enableFastLoad();
        CustomDialog::show(brls::getStr("home/deletingIconCache"), {}, [] {});

        m_deleteIconCacheTask = util::async([](std::stop_token) {
            StoreGameIconCache::deleteCache();
            brls::sync([] {
                deviceControl::CpuBoost::disable();
                deviceControl::HomeButton::enable();
                CustomDialog::show(brls::getStr("home/deleteIconCacheComplete"), {{brls::getStr("home/ok"), [] { CustomDialog::close(); }}});
            });
        });
    };

    CustomDialog::show(brls::getStr("home/deleteIconCacheConfirm"), {
        {brls::getStr("home/cancel"), [] { CustomDialog::close(); }},
        {brls::getStr("home/confirm"), onConfirmDelete},
    });
}

brls::Box* Home::createResetStateBox() {
    LongTextBoxConfig content;

    auto& reset = content.addEntry();
    reset.addTitle(brls::getStr("home/resetState"));
    reset.addBody(brls::getStr("home/resetStateBody"));

    auto& cases = content.addEntry();
    cases.addTitle(brls::getStr("home/resetStateCasesTitle"));
    cases.addBody(brls::getStr("home/resetStateCasesBody"), 1.3f);

    auto& warning = content.addEntry();
    warning.addTitle(brls::getStr("home/resetStateWarningTitle"));
    warning.addBody(brls::getStr("home/resetStateWarningBody"));

    return LongTextBox::create(content);
}

void Home::resetState() {
    auto onConfirm = [this] {
        deviceControl::HomeButton::disable();
        ProgressDialog::show(brls::getStr("home/resetting"), {}, nullptr);

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
                CustomDialog::show(brls::getStr("home/resetComplete"), {{brls::getStr("home/ok"), [] { CustomDialog::close(); }}});
            });
        });
    };

    LongPressDialog::show(createResetStateBox(), brls::getStr("home/resetStateBtn"), 3.0f, onConfirm, [] { LongPressDialog::close(); });
}

void Home::setNickname() {
    std::string current = Settings::getString("modShop", "nickname");
    std::string name = keyboard::showText(brls::getStr("home/inputNickname"), brls::getStr("home/inputNickname"), current, 32);
    if (name.empty()) return;
    Settings::setString("modShop", "nickname", name);
}

void Home::setupGameNameMenu() {
    m_gameNameMenu.title = brls::getStr("home/gameNameMenuTitle");

    auto& manualRenameItem = m_gameNameMenu.addItem(brls::getStr("home/manualRename"), brls::getStr("home/manualRenameDesc"));
    manualRenameItem.setDisabled([this]{ return m_gameManager.games().empty(); });
    manualRenameItem.setAction([this]{ manualSetGameDisplayName(); });

    auto& onlineFetchNameItem = m_gameNameMenu.addItem(brls::getStr("home/onlineFetch"), brls::getStr("home/onlineFetchDesc"));
    onlineFetchNameItem.setDisabled([this] { return m_gameManager.games().empty() || !deviceInfo::Network::isAvailable(); });
    onlineFetchNameItem.setBadge([] { return deviceInfo::Network::isAvailable() ? "" : brls::getStr("home/noNetwork"); });
    onlineFetchNameItem.setAsyncAction([this](std::stop_token token) -> std::any { return fetchGameDisplayName(token); });
    onlineFetchNameItem.setAction([this](std::any result) { checkFetchedGameDisplayName(result); });

    auto& restoreNameItem = m_gameNameMenu.addItem(brls::getStr("home/restoreName"), brls::getStr("home/restoreNameDesc"));
    restoreNameItem.setDisabled([this]{ return m_gameManager.games().empty(); });
    restoreNameItem.setBadge([this]() {
        if (m_gameManager.games().empty()) return std::string();
        return m_gameManager.getRestoredDisplayName(m_focusedIndex.load());
    });
    restoreNameItem.setAction([this]{ resetGameDisplayName(); });
}

void Home::setupGameManageMenu() {
    m_gameManageMenu.title = brls::getStr("home/gameManageMenuTitle");

    auto& renameItem = m_gameManageMenu.addItem(brls::getStr("home/renameSubmenu"), brls::getStr("home/renameSubmenuDesc"));
    renameItem.setDisabled([this]{ return m_gameManager.games().empty(); });
    renameItem.setBadge("\uE14A");
    renameItem.setSubmenu(&m_gameNameMenu);

    auto& removeGameItem = m_gameManageMenu.addItem(brls::getStr("home/removeGame"), brls::getStr("home/removeGameDesc"));
    removeGameItem.setDisabled([this]{ return m_gameManager.games().empty(); });
    removeGameItem.setAction([this]{ removeGame(); });

    auto& viewPathItem = m_gameManageMenu.addItem(brls::getStr("home/viewPath"), brls::getStr("home/viewPathDesc"));
    viewPathItem.setDisabled([this]{ return m_gameManager.games().empty(); });
    viewPathItem.setBadge([this]() {
        if (m_gameManager.games().empty()) return std::string();
        return m_gameManager.getDirName(m_focusedIndex.load());
    });
    viewPathItem.setAction([this]{ CustomDialog::show(m_gameManager.games()[m_focusedIndex.load()].dirPath, {{brls::getStr("home/ok"), [] { CustomDialog::close(); }}}); });
}

void Home::setupSettingsMenu() {
    m_settingsMenu.title = brls::getStr("home/settingsMenuTitle");
    m_basicSettingsMenu.title = brls::getStr("home/basicSettings");
    m_advancedSettingsMenu.title = brls::getStr("home/advancedSettings");
    m_assistFeaturesMenu.title = brls::getStr("home/assistFeatures");

    auto& nicknameItem = m_settingsMenu.addItem(brls::getStr("home/nickname"), brls::getStr("home/nicknameDesc"));
    nicknameItem.setBadge([]{ return Settings::getString("modShop", "nickname", brls::getStr("home/anonymousUser")); });
    nicknameItem.setAction([this]{ setNickname(); });
    nicknameItem.setStayOpen();

    auto& basicItem = m_settingsMenu.addItem(brls::getStr("home/basicSettings"), brls::getStr("home/basicSettingsDesc"));
    basicItem.setBadge("\uE14A");
    basicItem.setSubmenu(&m_basicSettingsMenu);

    auto& advancedItem = m_settingsMenu.addItem(brls::getStr("home/advancedSettings"), brls::getStr("home/advancedSettingsDesc"));
    advancedItem.setBadge("\uE14A");
    advancedItem.setSubmenu(&m_advancedSettingsMenu);

    auto& assistItem = m_settingsMenu.addItem(brls::getStr("home/assistFeatures"), brls::getStr("home/assistFeaturesDesc"));
    assistItem.setBadge("\uE14A");
    assistItem.setSubmenu(&m_assistFeaturesMenu);

    auto& langItem = m_basicSettingsMenu.addItem(brls::getStr("home/language"), brls::getStr("home/languageDesc"));
    langItem.setBadge([]{
        std::string l = Settings::getString("UI", "language", "auto");
        if (l == "zh-Hans") return brls::getStr("home/langZhCNItem");
        if (l == "zh-Hant") return brls::getStr("home/langZhTWItem");
        if (l == "en-US")   return brls::getStr("home/langEnUSItem");
        return brls::getStr("home/langAutoItem");
    });
    langItem.setSubmenu(&m_langMenu);

    auto& themeItem = m_basicSettingsMenu.addItem(brls::getStr("home/themeColor"), brls::getStr("home/themeColorDesc"));
    themeItem.setBadge([]{
        std::string t = Settings::getString("UI", "theme", "auto");
        if (t == "light") return brls::getStr("home/themeLightItem");
        if (t == "dark")  return brls::getStr("home/themeDarkItem");
        return brls::getStr("home/themeAuto");
    });
    themeItem.setSubmenu(&m_themeMenu);

    auto& soundItem = m_basicSettingsMenu.addItem(brls::getStr("home/soundEffect"), brls::getStr("home/soundEffectDesc"));
    soundItem.setBadge([]{ return Settings::getBool("Audio", "muted", false) ? brls::getStr("home/off") : brls::getStr("home/on"); });
    soundItem.setBadgeHighlight([]{ return !Settings::getBool("Audio", "muted", false); });
    soundItem.setStayOpen();
    soundItem.setAction([]{
        bool newVal = !Settings::getBool("Audio", "muted", false);
        Settings::setBool("Audio", "muted", newVal);
        Audio::instance()->setMuted(newVal);
    });

    auto& fpsItem = m_advancedSettingsMenu.addItem(brls::getStr("home/fpsMonitor"), brls::getStr("home/fpsMonitorDesc"));
    fpsItem.setBadge([]{ return Settings::getBool("UI", "showFps", false) ? brls::getStr("home/on") : brls::getStr("home/off"); });
    fpsItem.setBadgeHighlight([]{ return Settings::getBool("UI", "showFps", false); });
    fpsItem.setStayOpen();
    fpsItem.setAction([this]{
        bool newVal = !Settings::getBool("UI", "showFps", false);
        Settings::setBool("UI", "showFps", newVal);
        m_frame->setShowFps(newVal);
    });

    auto& memItem = m_advancedSettingsMenu.addItem(brls::getStr("home/memMonitor"), brls::getStr("home/memMonitorDesc"));
    memItem.setBadge([]{ return Settings::getBool("UI", "showMem", false) ? brls::getStr("home/on") : brls::getStr("home/off"); });
    memItem.setBadgeHighlight([]{ return Settings::getBool("UI", "showMem", false); });
    memItem.setStayOpen();
    memItem.setAction([this] {
        bool newVal = !Settings::getBool("UI", "showMem", false);
        Settings::setBool("UI", "showMem", newVal);
        m_frame->setShowMem(newVal);
    });

    auto& cpuBoostItem = m_advancedSettingsMenu.addItem(brls::getStr("home/cpuBoost"), brls::getStr("home/cpuBoostDesc"));
    cpuBoostItem.setBadge([]{ return Settings::getBool("Performance", "cpuBoost", true) ? brls::getStr("home/on") : brls::getStr("home/off"); });
    cpuBoostItem.setBadgeHighlight([]{ return Settings::getBool("Performance", "cpuBoost", true); });
    cpuBoostItem.setStayOpen();
    cpuBoostItem.setAction([]{
        bool newVal = !Settings::getBool("Performance", "cpuBoost", true);
        Settings::setBool("Performance", "cpuBoost", newVal);
    });

    auto& transitItem = m_assistFeaturesMenu.addItem(brls::getStr("home/clearTransitItem"), brls::getStr("home/clearTransitDesc"));
    transitItem.setBadge([this]() {
        int count = ModManager::transitModCount();
        return count > 0 ? std::to_string(count) + " MOD" : "0 MOD";
    });
    transitItem.setAction([this]{ clearTransit(); });

    auto& iconCacheItem = m_assistFeaturesMenu.addItem(brls::getStr("home/deleteIconCacheItem"), brls::getStr("home/deleteIconCacheDesc"));
    iconCacheItem.setAction([this]{ deleteIconCache(); });

    auto& resetItem = m_assistFeaturesMenu.addItem(brls::getStr("home/resetState"), brls::getStr("home/resetStateBody"));
    resetItem.setAction([this]{ resetState(); });
}

void Home::setupLangMenu() {
    m_langMenu.title = brls::getStr("home/langMenuTitle");

    auto currentLang = []{ return Settings::getString("UI", "language", "auto"); };
    auto applyLang = [](const std::string& value) {
        Settings::setString("UI", "language", value);
        CustomDialog::show(brls::getStr("home/langRestartMsg"), {
            {brls::getStr("home/cancel"), []{ CustomDialog::close(); }},
            {brls::getStr("home/restart"), []{
                CustomDialog::show(brls::getStr("home/restarting"), {}, []{});
                envSetNextLoad(config::getNroPath(), config::getNroPath());
                brls::Application::quit();
            }},
        });
    };

    auto& autoItem = m_langMenu.addItem(brls::getStr("home/langAutoItem"), brls::getStr("home/langAutoDesc"));
    autoItem.setBadge([=]{ return currentLang() == "auto" ? "\uE14B" : ""; });
    autoItem.setStayOpen();
    autoItem.setAction([=]{ applyLang("auto"); });

    auto& zhCNItem = m_langMenu.addItem(brls::getStr("home/langZhCNItem"), brls::getStr("home/langZhCNDesc"));
    zhCNItem.setBadge([=]{ return currentLang() == "zh-Hans" ? "\uE14B" : ""; });
    zhCNItem.setStayOpen();
    zhCNItem.setAction([=]{ applyLang("zh-Hans"); });

    auto& zhTWItem = m_langMenu.addItem(brls::getStr("home/langZhTWItem"), brls::getStr("home/langZhTWDesc"));
    zhTWItem.setBadge([=]{ return currentLang() == "zh-Hant" ? "\uE14B" : ""; });
    zhTWItem.setStayOpen();
    zhTWItem.setAction([=]{ applyLang("zh-Hant"); });

    auto& enUSItem = m_langMenu.addItem(brls::getStr("home/langEnUSItem"), brls::getStr("home/langEnUSDesc"));
    enUSItem.setBadge([=]{ return currentLang() == "en-US" ? "\uE14B" : ""; });
    enUSItem.setStayOpen();
    enUSItem.setAction([=]{ applyLang("en-US"); });
}

void Home::setupThemeMenu() {
    m_themeMenu.title = brls::getStr("home/themeMenuTitle");

    auto currentTheme = []{ return Settings::getString("UI", "theme", "auto"); };
    auto applyTheme = [](const std::string& value) {
        Settings::setString("UI", "theme", value);
        CustomDialog::show(brls::getStr("home/themeRestartMsg"), {
            {brls::getStr("home/cancel"), []{ CustomDialog::close(); }},
            {brls::getStr("home/restart"), []{
                CustomDialog::show(brls::getStr("home/restarting"), {}, []{});
                envSetNextLoad(config::getNroPath(), config::getNroPath());
                brls::Application::quit();
            }},
        });
    };

    auto& themeAutoItem = m_themeMenu.addItem(brls::getStr("home/themeAuto"), brls::getStr("home/themeAutoDesc"));
    themeAutoItem.setBadge([=]{ return currentTheme() == "auto" ? "\uE14B" : ""; });
    themeAutoItem.setStayOpen();
    themeAutoItem.setAction([=]{ applyTheme("auto"); });

    auto& themeLightItem = m_themeMenu.addItem(brls::getStr("home/themeLightItem"), brls::getStr("home/themeLightDesc"));
    themeLightItem.setBadge([=]{ return currentTheme() == "light" ? "\uE14B" : ""; });
    themeLightItem.setStayOpen();
    themeLightItem.setAction([=]{ applyTheme("light"); });

    auto& themeDarkItem = m_themeMenu.addItem(brls::getStr("home/themeDarkItem"), brls::getStr("home/themeDarkDesc"));
    themeDarkItem.setBadge([=]{ return currentTheme() == "dark" ? "\uE14B" : ""; });
    themeDarkItem.setStayOpen();
    themeDarkItem.setAction([=]{ applyTheme("dark"); });
}

void Home::setupAddProjectMenu() {
    m_addProjectMenu.title = brls::getStr("home/addProjectSubmenu");

    auto& storeAddItem = m_addProjectMenu.addItem(brls::getStr("home/storeAddSubmenu"),
        brls::getStr("home/storeAddSubmenuDesc"));
    storeAddItem.setBadge("\uE14A");
    storeAddItem.setSubmenu(&m_storeAddMenu);

    auto& localAddItem = m_addProjectMenu.addItem(brls::getStr("home/localAdd"),
        brls::getStr("home/addGameDesc"));
    localAddItem.setBadge("\uE14A");
    localAddItem.setAction([this]{ openAddGamePage(); });
}

void Home::setupStoreAddMenu() {
    m_storeAddMenu.title = brls::getStr("home/storeAddSubmenu");
    m_storeAddMenu.shouldShowFakeHighlight = [this]{ return !m_gameManager.games().empty(); };

    auto& gameStoreItem = m_storeAddMenu.addItem(brls::getStr("home/gameStore"),
        brls::getStr("home/gameStoreDesc"));
    gameStoreItem.setBadge("\uE14A");
    gameStoreItem.setAction([this] {
        pageNav::push(new StoreGameList(m_gameManager), pageNav::Anim::SLIDE_LEFT);
    });

    auto& modStoreItem = m_storeAddMenu.addItem(brls::getStr("home/modStore"),
        brls::getStr("home/modStoreDesc"));
    modStoreItem.setDisabled([this] { return m_gameManager.games().empty(); });
    modStoreItem.setBadge("\uE14A");
    modStoreItem.setAction([this] {
        auto& game = m_gameManager.games()[m_focusedIndex.load()];
        std::string tid = format::appIdHex(game.appId);
        pageNav::push(new StoreModList(tid, game.displayName, m_gameManager, nullptr, game.version), pageNav::Anim::SLIDE_LEFT);
    });

    auto& uploadItem = m_storeAddMenu.addItem(brls::getStr("home/uploadMod"), brls::getStr("home/uploadModDesc"));
    uploadItem.setBadge("\uE14A");
    uploadItem.setAction([] {
        QrCodeView::show(config::websiteUrl);
    });
    uploadItem.setStayOpen();
}

void Home::setupSortFilterMenu() {
    m_sortFilterMenu.title = brls::getStr("home/sortFilterTitle");

    auto applySortMode = [this](SortMode mode) {
        m_gameManager.setSortMode(mode);
        m_grid->setDefaultCellFocus(0);
        m_grid->reloadData();
        m_grid->instantFocus(0);
        m_frame->updateActionHint(brls::BUTTON_Y, m_gameManager.sortAsc() ? brls::getStr("home/sortAsc") : brls::getStr("home/sortDesc"));
        brls::Application::getGlobalHintsUpdateEvent()->fire();
    };

    auto& sortNameItem = m_sortFilterMenu.addItem(brls::getStr("home/sortByName"), brls::getStr("home/sortByNameDesc"));
    sortNameItem.setBadge([this]{ return m_gameManager.sortMode() == SortMode::Name ? "\uE14B" : ""; });
    sortNameItem.setAction([applySortMode]{ applySortMode(SortMode::Name); });

    auto& sortCountItem = m_sortFilterMenu.addItem(brls::getStr("home/sortByCount"), brls::getStr("home/sortByCountDesc"));
    sortCountItem.setBadge([this]{ return m_gameManager.sortMode() == SortMode::ModCount ? "\uE14B" : ""; });
    sortCountItem.setAction([applySortMode]{ applySortMode(SortMode::ModCount); });

    auto& sortRecentItem = m_sortFilterMenu.addItem(brls::getStr("home/sortByRecent"), brls::getStr("home/sortByRecentDesc"));
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
    m_menu.title = brls::getStr("home/mainMenuTitle");

    m_menu.shouldShowFakeHighlight = [this]{ return !m_gameManager.games().empty(); };

    auto& addProjectItem = m_menu.addItem(brls::getStr("home/addProjectSubmenu"),
        brls::getStr("home/addProjectSubmenuDesc"));
    addProjectItem.setBadge("\uE14A");
    addProjectItem.setSubmenu(&m_addProjectMenu);

    auto& manageSubmenuItem = m_menu.addItem(brls::getStr("home/manageSubmenu"), brls::getStr("home/manageSubmenuDesc"));
    manageSubmenuItem.setBadge("\uE14A");
    manageSubmenuItem.setSubmenu(&m_gameManageMenu);

    auto& sortFilterItem = m_menu.addItem(brls::getStr("home/sortFilter"), brls::getStr("home/sortFilterDesc"));
    sortFilterItem.setBadge("\uE14A");
    sortFilterItem.setSubmenu(&m_sortFilterMenu);
    sortFilterItem.setDisabled([this]{ return !m_nacpComplete || m_gameManager.games().empty(); });

    auto& settingsItem = m_menu.addItem(brls::getStr("home/settingsSubmenu"), brls::getStr("home/settingsSubmenuDesc"));
    settingsItem.setBadge("\uE14A");
    settingsItem.setSubmenu(&m_settingsMenu);

    auto& aboutItem = m_menu.addItem(brls::getStr("home/aboutItem"), brls::getStr("home/aboutItemDesc"));
    aboutItem.setBadge("\uE14A");
    aboutItem.setAction([]{ pageNav::push(new Help()); });

    auto& updateItem = m_menu.addItem(brls::getStr("home/checkUpdate"), brls::getStr("home/checkUpdateDesc"));
    updateItem.setDisabled([]{ return !deviceInfo::Network::isAvailable(); });
    updateItem.setBadge([]() -> std::string {
        if (!deviceInfo::Network::isAvailable()) return brls::getStr("home/noNetwork");
        return AppUpdater::instance().hasUpdate() ? brls::getStr("home/updateFound", AppUpdater::instance().tagName()) : "\uE14A";
    });
    updateItem.setAsyncAction([this](std::stop_token token) -> std::any { return checkForUpdate(token); });
    updateItem.setAction([this](std::any result) { showManualUpdateResult(result); });

    m_frame->registerAction(brls::getStr("home/menu"), brls::BUTTON_X, [this](...) {
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

            auto stack = brls::Application::getActivitiesStack();
            if (stack.empty() || stack.back() != this) return;

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
    std::string updateTitle = brls::getStr("home/updateDialogTitle") + updater.tagName();
    std::string updateBody =
        brls::getStr("home/updateDialogPublished") + updater.publishTime() + "\n" +
        brls::getStr("home/updateDialogChangelogBelow");

    LongTextBoxConfig content;

    auto& update = content.addEntry();
    update.addTitle(updateTitle);
    update.addBody(updateBody);

    auto& support = content.addEntry();
    support.addTitle(brls::getStr("home/updateDialogSupport"));
    support.addBody(brls::getStr("home/updateDialogSupportDesc"));
    support.addQr(config::donateWechatQr, brls::getStr("home/updateDialogQrWechat"));
    support.addQr(config::donatePaypalQr, brls::getStr("home/updateDialogQrPaypal"));

    auto& community = content.addEntry();
    community.addTitle(brls::getStr("home/updateDialogCommunity"));
    community.addQr(config::communityQqQr, brls::getStr("home/updateDialogQrQQ"));
    community.addQr(config::communityDiscordQr, brls::getStr("home/updateDialogQrDiscord"));

    auto& changelog = content.addEntry();
    changelog.addTitle(brls::getStr("home/updateDialogChangelog"));
    changelog.addBody(releaseNotes.empty() ? brls::getStr("home/updateDialogNoChangelog") : releaseNotes, 1.3f);

    return LongTextBox::create(content);
}

void Home::showForcedUpdateDialog() {
    Audio::instance()->play(SoundEffect::Enter);
    ScrollDialog::show(
        createUpdateDetailBox(),
        brls::getStr("home/exit"), [] { brls::Application::quit(); },
        brls::getStr("home/updateBtn"), [this] { startUpdateDownload(); },
        [] { brls::Application::quit(); });
}

void Home::showManualUpdateResult(std::any result) {
    if (!AppUpdater::instance().hasUpdate()) {
        CustomDialog::show(brls::getStr("home/upToDate"), {{brls::getStr("home/ok"), []{ CustomDialog::close(); }}});
        return;
    }

    ScrollDialog::show(
        createUpdateDetailBox(),
        brls::getStr("home/cancel"), [] { ScrollDialog::close(); },
        brls::getStr("home/updateBtn"), [this] { startUpdateDownload(); },
        [] { ScrollDialog::close(); });
}

void Home::startUpdateDownload() {
    deviceControl::HomeButton::disable();
    ProgressDialog::show(brls::getStr("home/downloading"), {}, [] {});
    ProgressDialog::setLeftText(brls::getStr("home/calculating"));
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
                CustomDialog::show(result.error, {{brls::getStr("home/ok"), []{ CustomDialog::close(); }}});
                return;
            }
            CustomDialog::show(brls::getStr("home/restarting"), {}, []{});
            brls::sync([] {
                AppUpdater::instance().install();
                envSetNextLoad(config::getNroPath(), config::getNroPath());
                brls::Application::quit();
            });
        });
    });
}

void Home::showDuplicateWarning(int duplicateCount) {
    std::string msg = brls::getStr("home/duplicateWarning", duplicateCount);
    Audio::instance()->play(SoundEffect::Enter);
    CustomDialog::show(msg, {{brls::getStr("home/ok"), [] { CustomDialog::close(); }}});
}

brls::Box* Home::createFirstLaunchBox() {
    LongTextBoxConfig content;

    auto& notice = content.addEntry();
    notice.addTitle(brls::getStr("home/firstLaunchNoticeTitle"));
    notice.addBody(brls::getStr("home/firstLaunchNoticeBody"));

    auto& quickStart = content.addEntry();
    quickStart.addTitle(brls::getStr("home/firstLaunchQuickStartTitle"));
    quickStart.addBody(brls::getStr("home/firstLaunchQuickStartBody"));

    auto& disclaimer = content.addEntry();
    disclaimer.addTitle(brls::getStr("home/firstLaunchDisclaimerTitle"));
    disclaimer.addBody(brls::getStr("home/firstLaunchDisclaimerBody"));

    return LongTextBox::create(content);
}

void Home::runStartupDialogs() {

    // onContentAvailable 处于 pushActivity 执行中，当前 Activity 尚未入栈，
    // 直接 pushActivity(Dialog) 会导致栈序错乱，需延迟到下一帧执行

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

        LongPressDialog::show(createFirstLaunchBox(), brls::getStr("home/firstLaunchBtn"), 5.0f, onConfirm);
    });
}

