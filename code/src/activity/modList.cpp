/**
 * ModList - Mod 列表页面
 * 左侧 RecyclingGrid（单列 Mod 列表） + 右侧 Mod 详情
 */

#include "activity/modList.hpp"
#include <borealis/core/i18n.hpp>
#include "core/audio.hpp"
#include "view/modCard.hpp"
#include "dataSource/modCardDS.hpp"
#include "utils/format.hpp"
#include "utils/fsHelper.hpp"
#include "view/customDialog.hpp"
#include "view/progressDialog.hpp"
#include "view/mtpDialog.hpp"
#include "view/ftpDialog.hpp"
#include "view/qrCodeView.hpp"
#include "activity/storeModList.hpp"
#include "activity/storeModDetail.hpp"
#include "common/settings.hpp"
#include "common/config.hpp"
#include "utils/keyboard.hpp"
#include "utils/pageNav.hpp"
#include "core/homeBtnBlock.hpp"
#include <borealis/core/cache_helper.hpp>
#include <chrono>
#include <yoga/Yoga.h>
#include <switch.h>


ModList::ModList(size_t gameIndex, GameManager& gameManager)
    : m_gameManager(gameManager), m_gameIndex(gameIndex), m_modManager(gameManager.games()[gameIndex]) {

}

ModList::~ModList() {
    m_alive->store(false);
}

void ModList::onResume() {
    int modID = m_modManager.consumePendingFocus();
    if (modID < 0) return;
    m_modManager.sort(m_sortAsc);
    int newIdx = m_modManager.findByModID(modID);
    if (newIdx >= 0) {
        m_grid->deferReload(newIdx);
        m_focusedIndex = newIdx;
    }
}

void ModList::onContentAvailable() {
    m_frame->setTitle(m_modManager.game().displayName);
    setupDetail();
    setupModGrid();

    if (!m_modManager.mods().empty()) updateDetail(0);

    m_frame->registerAction(brls::getStr("modList/sortAsc"), brls::BUTTON_Y, [this](...) {
        Audio::instance()->play(SoundEffect::Click);
        toggleSort();
        return true;
    });

    setupMenu();
    setupStoreMenu();

    m_frame->registerAction(brls::getStr("modList/menu"), brls::BUTTON_X, [this](...) {
        Audio::instance()->play(SoundEffect::Enter);
        m_menu.show();
        return true;
    });

    m_frame->registerAction(brls::getStr("modList/store"), brls::BUTTON_START, [this](...) {
        Audio::instance()->play(SoundEffect::Enter);
        m_storeMenu.show();
        return true;
    });

    startSizeLoader();
}

void ModList::toggleModInstall(int index) {
    auto& mod = m_modManager.mods()[index];
    bool installing = !mod.isInstalled;
    std::string modName = mod.displayName;
    std::string dirName = mod.dirName;

    auto onConfirm = [this, index, installing, modName, dirName] {
        HomeBtnBlock::disable();
        std::string title = installing ? brls::getStr("modList/installing", modName) : brls::getStr("modList/uninstalling", modName);
        if (installing) {
            auto onCancel = [this] { m_installTask.request_stop(); };
            ProgressDialog::show(title, {{brls::getStr("modList/cancel"), onCancel}}, onCancel);
        } else {
            ProgressDialog::show(title, {}, [] {});
        }

        auto alive = m_alive;
        auto cleaningStarted = std::make_shared<bool>(false);

        auto progressCb = [alive, cleaningStarted, modName](const ModInstaller::Progress& progress) {
            bool cleaning    = progress.cleaning;
            int current      = progress.current;
            int total        = progress.total;
            std::string file = progress.currentFile;
            int64_t written  = progress.bytesWritten;
            int64_t fileSize = progress.bytesTotal;

            brls::sync([=] {
                if (!alive->load()) return;

                if (cleaning && !*cleaningStarted) {
                    *cleaningStarted = true;
                    ProgressDialog::setTitle(brls::getStr("modList/cleaning", modName));
                    ProgressDialog::hideButtons();
                }

                ProgressDialog::setLeftText(file);
                if (total > 0) ProgressDialog::setRightText(std::to_string(current) + " / " + std::to_string(total));
                if (current > 0 && total > 0) ProgressDialog::setMainProgress(current * 100.0f / total);
                if (!*cleaningStarted && written > 0 && fileSize > 0) ProgressDialog::setSubProgress(written, fileSize);
                else ProgressDialog::hideSubProgress();
            });
        };

        m_installTask = util::async([this, alive, index, installing, dirName, progressCb](std::stop_token token) {
            using Clock = std::chrono::steady_clock;
            auto startTime = Clock::now();

            bool success = false;
            std::string errorMsg, errorFile, conflictMod;

            if (installing) {
                auto result = m_modManager.installMod(index, progressCb, token);
                success     = result.success;
                errorMsg    = std::move(result.errorMsg);
                errorFile   = std::move(result.errorFile);
                conflictMod = std::move(result.conflictMod);
            } else {
                auto result = m_modManager.uninstallMod(index, progressCb);
                success     = result.success;
                errorMsg    = std::move(result.errorMsg);
                errorFile   = std::move(result.errorFile);
            }

            auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - startTime).count();
            std::string elapsed = format::elapsed(elapsedMs);

            bool cancelled = token.stop_requested();
            brls::sync([this, alive, installing, success, cancelled, errorMsg = std::move(errorMsg), errorFile = std::move(errorFile), conflictMod = std::move(conflictMod), dirName, elapsed] {
                HomeBtnBlock::enable();
                if (!alive->load()) return;

                if (success) {
                    m_gameManager.setHasInstalledMod(m_gameIndex, installing || ModManager::hasInstalledMod(m_modManager.game().dirPath));
                    std::string msg = installing ? brls::getStr("modList/installSuccess", elapsed) : brls::getStr("modList/uninstallSuccess", elapsed);
                    m_modManager.sort(m_sortAsc);
                    int focusIdx = std::max(0, m_modManager.findByDirName(dirName));
                    CustomDialog::show(msg, {{brls::getStr("modList/ok"), [this, focusIdx] {CustomDialog::close([this, focusIdx] { refreshAndFocus(focusIdx); });}}});
                    return;
                }

                std::string msg;
                if (cancelled) msg = brls::getStr("modList/installCancelled");
                else if (!conflictMod.empty()) msg = brls::getStr("modList/installConflict", conflictMod, errorFile);
                else msg = (installing ? brls::getStr("modList/installFailed", errorMsg, errorFile) : brls::getStr("modList/uninstallFailed", errorMsg, errorFile));
                CustomDialog::show(msg, {{brls::getStr("modList/ok"), [] { CustomDialog::close(); }}});
            });
        });
    };

    CustomDialog::show(installing ? brls::getStr("modList/confirmInstall") : brls::getStr("modList/confirmUninstall"), {
        {brls::getStr("modList/cancel"), [] { CustomDialog::close(); }},
        {brls::getStr("modList/confirm"), onConfirm},
    });
}

void ModList::toggleModDisable() {
    bool disabling = !m_modManager.game().isModsDisabled;

    auto onConfirm = [this, disabling] {
        std::string msg = disabling ? brls::getStr("modList/disabling") : brls::getStr("modList/enabling");
        CustomDialog::show(msg, {}, [] {});

        // 禁用时先写标志再做文件操作：异常中断后标志已是禁用状态，下次启动仍可走恢复流程
        if (disabling) m_gameManager.setModsDisabled(m_gameIndex, true);

        auto alive = m_alive;
        m_installTask = util::async([this, alive, disabling](std::stop_token) {
            bool ok;
            if (disabling) ok = m_modManager.disableMods();
            else ok = m_modManager.enableMods();

            brls::sync([this, alive, disabling, ok] {
                if (!alive->load()) return;
                if (!ok) {
                    CustomDialog::show(brls::getStr("modList/disableFailed"), {{brls::getStr("modList/ok"), [] { CustomDialog::close(); }}});
                    return;
                }
                if (!disabling) m_gameManager.setModsDisabled(m_gameIndex, false);
                std::string doneMsg = disabling ? brls::getStr("modList/disableSuccess") : brls::getStr("modList/enableSuccess");
                CustomDialog::show(doneMsg, {{brls::getStr("modList/ok"), [this] { CustomDialog::close([this] { refreshAndFocus(m_focusedIndex.load()); }); }}});
            });
        });
    };

    std::string confirmMsg = disabling ? brls::getStr("modList/confirmDisable") : brls::getStr("modList/confirmEnable");
    CustomDialog::show(confirmMsg, {
        {brls::getStr("modList/cancel"), [] { CustomDialog::close(); }},
        {brls::getStr("modList/confirm"), onConfirm},
    });
}

void ModList::setupModGrid() {
    m_grid->setPadding(25, 0, 25, 40);
    m_grid->setScrollingIndicatorVisible(false);
    m_grid->registerCell("ModCard", ModCard::create);

    auto* ds = new ModCardDS(m_modManager.mods(), m_modManager.game().isModsDisabled, [this](size_t index) {
        if (m_modManager.game().isModsDisabled) return;
        toggleModInstall(static_cast<int>(index));
    });
    m_grid->setDataSource(ds);

    m_grid->setFocusChangeCallback([this](size_t index) {
        m_focusedIndex.store(index);
        m_frame->setIndexText(std::to_string(index + 1) + " / " + std::to_string(m_modManager.mods().size()));
        updateDetail(index);
    });

    // 右键：列表 → 详情面板
    m_grid->registerAction("", brls::BUTTON_NAV_RIGHT, [this](...) {
        Audio::instance()->play(SoundEffect::Focus);
        brls::Application::giveFocus(m_detail);
        return true;
    }, true, true);
}

void ModList::toggleSort() {
    m_sortAsc = !m_sortAsc;
    m_modManager.sort(m_sortAsc);
    refreshAndFocus(0);
    m_frame->updateActionHint(brls::BUTTON_Y, m_sortAsc ? brls::getStr("modList/sortAsc") : brls::getStr("modList/sortDesc"));
    brls::Application::getGlobalHintsUpdateEvent()->fire();
}

void ModList::refreshAndFocus(int index) {
    m_grid->setDefaultCellFocus(index);
    m_grid->reloadData();
    m_grid->instantFocus(index);
    m_focusedIndex = index;
    updateDetail(index);
}

void ModList::setupDetail() {
    // 标签区域启用自动换行（Borealis XML 不支持 flexWrap 属性，直接调 Yoga API）
    YGNodeStyleSetFlexWrap(m_tagRow->getYGNode(), YGWrapWrap);

    // 隐藏描述区滚动条（scrollingIndicatorVisible 非 XML 注册属性）
    m_scroll->setScrollingIndicatorVisible(false);

    // 左键：详情面板 → 列表（重置滚动 + 恢复焦点位置）
    m_detail->registerAction("", brls::BUTTON_NAV_LEFT, [this](...) {
        Audio::instance()->play(SoundEffect::Focus);
        m_scroll->setContentOffsetY(0, false);
        auto* cell = m_grid->getGridItemByIndex(m_lastFocusIndex);
        if (cell) brls::Application::giveFocus(cell);
        else brls::Application::giveFocus(m_grid);
        return true;
    }, true, true);

    // 右键：详情面板边界
    m_detail->registerAction("", brls::BUTTON_NAV_RIGHT, [this](...) {
        Audio::instance()->play(SoundEffect::FocusLimit);
        m_detail->shakeHighlight(brls::FocusDirection::RIGHT);
        return true;
    }, true);

    // 上下键：animated 驱动描述区滚动
    m_detail->registerAction("", brls::BUTTON_NAV_DOWN, [this](...) {
        float cur = m_scroll->getContentOffsetY();
        m_scroll->setContentOffsetY(cur + 60.0f, true);
        return true;
    }, true, true);
    m_detail->registerAction("", brls::BUTTON_NAV_UP, [this](...) {
        float cur = m_scroll->getContentOffsetY();
        m_scroll->setContentOffsetY(cur - 60.0f, true);
        return true;
    }, true, true);

    // 游戏名和 TID
    auto& game = m_modManager.game();
    m_gameNameLabel->setText(game.displayName);
    std::string tid = format::appIdHex(game.appId);
    m_gameTid->setText(tid);

    // 从 GameInfo 或缓存取游戏图标
    if (game.iconId > 0) m_gameIcon->innerSetImage(game.iconId);
    else startIconLoader();

    // 收藏标志
    m_favIcon->setVisibility(game.isFavorite ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
}

void ModList::updateDetail(size_t index) {
    if (index >= m_modManager.mods().size()) return;
    m_lastFocusIndex = index;
    m_scroll->setContentOffsetY(0, false);
    auto& mod = m_modManager.mods()[index];
    m_tagType->setText(modTypeText(mod.type));
    m_tagAuthor->setText(mod.author.empty() ? brls::getStr("modList/unknownAuthor") : brls::getStr("modList/authorPrefix", mod.author));
    m_tagFormat->setText(mod.isZip ? brls::getStr("modList/zipType") : brls::getStr("modList/fileType"));
    m_tagSize->setText(mod.size.empty() ? brls::getStr("modList/calculatingSize") : mod.size);
    m_tagVersion->setText(mod.modVersion.empty() ? brls::getStr("modList/modVersionUnknown") : brls::getStr("modList/modVersionFmt", mod.modVersion));
    if (mod.gameVersion.empty()) m_tagGameVer->setText(brls::getStr("modList/gameVersionUnknown"));
    else if (mod.gameVersion == "0") m_tagGameVer->setText(brls::getStr("modList/gameVersionUniversal"));
    else m_tagGameVer->setText(brls::getStr("modList/gameVersionFmt", mod.gameVersion));
    m_descBody->setText(mod.description.empty() ? brls::getStr("modList/noDescription") : mod.description);
}

void ModList::startIconLoader() {
    m_iconLoader = util::async([this](std::stop_token token) {
        std::string tid = format::appIdHex(m_modManager.game().appId);
        for (int retry = 0; retry < 10 && !token.stop_requested(); retry++) {
            for (int i = 0; i < 100 && !token.stop_requested(); i++) svcSleepThread(10000000ULL);  // 10ms × 100 = 1s
            if (token.stop_requested()) break;
            int iconId = brls::TextureCache::instance().getCache(tid);
            if (iconId > 0) {
                auto alive = m_alive;
                brls::sync([this, alive, iconId]() {
                    if (!alive->load()) return;
                    m_gameIcon->innerSetImage(iconId);
                });
                break;
            }
        }
    });
}

void ModList::startSizeLoader() {
    m_sizeLoader = util::async([this](std::stop_token token) {
        m_sizeLoading.store(true);

        std::vector<int> tasks;
        for (int i = 0; i < static_cast<int>(m_modManager.mods().size()); i++) {
            if (m_modManager.mods()[i].size.empty()) tasks.push_back(i);
        }

        while (!tasks.empty() && !token.stop_requested()) {
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

            int modIdx = tasks[bestIdx];
            auto& mod = m_modManager.mods()[modIdx];
            std::string modPath = mod.path;
            tasks.erase(tasks.begin() + bestIdx);

            int64_t bytes = fs::calcDirSize(modPath, &token);
            if (token.stop_requested()) break;
            if (bytes < 0) continue;
            std::string sizeStr = format::fileSize(bytes);

            // 回主线程更新数据和 UI
            auto alive = m_alive;
            brls::sync([this, alive, modIdx, sizeStr = std::move(sizeStr)]() {
                if (!alive->load()) return;
                applySizeResult(modIdx, sizeStr);
            });

            svcSleepThread(1000000ULL);  // 1ms
        }

        if (!token.stop_requested()) {
            auto alive = m_alive;
            brls::sync([this, alive]() {
                if (!alive->load()) return;
                m_modManager.saveJson();
            });
        }

        m_sizeLoading.store(false);
    });
}

void ModList::applySizeResult(int modIdx, const std::string& sizeStr) {
    m_modManager.setSize(modIdx, sizeStr);
    if (static_cast<size_t>(modIdx) == m_lastFocusIndex) m_tagSize->setText(sizeStr);
}

void ModList::applyModDisplayName(int idx, const std::string& name) {
    m_modManager.setDisplayName(idx, name);
    auto& mod = m_modManager.mods()[idx];
    auto* cell = m_grid->getGridItemByIndex(idx);
    if (cell) static_cast<ModCard*>(cell)->setMod(mod.displayName, mod.type, mod.isInstalled, m_modManager.game().isModsDisabled, mod.modID);
}

void ModList::manualSetModDisplayName() {
    int idx = m_focusedIndex.load();
    std::string name = keyboard::showText(brls::getStr("modList/inputModName"), brls::getStr("modList/inputModName"), m_modManager.mods()[idx].displayName, 64);
    if (name.empty()) return;
    applyModDisplayName(idx, name);
}

void ModList::resetModDisplayName() {
    int idx = m_focusedIndex.load();
    std::string restored = m_modManager.mods()[idx].dirName;
    auto onConfirm = [this, idx] {
        m_modManager.deleteCustomDisplayName(idx);
        auto& mod = m_modManager.mods()[idx];
        auto* cell = m_grid->getGridItemByIndex(idx);
        if (cell) static_cast<ModCard*>(cell)->setMod(mod.displayName, mod.type, mod.isInstalled, m_modManager.game().isModsDisabled, mod.modID);
    };
    CustomDialog::show(brls::getStr("modList/restoreNameMsg", restored), {
        {brls::getStr("modList/cancel"), [] { CustomDialog::close(); }},
        {brls::getStr("modList/restoreNameOk"), [onConfirm] { CustomDialog::close(onConfirm); }},
    });
}

void ModList::applyModType(int idx, const std::string& type) {
    m_modManager.setType(idx, type);
    auto& mod = m_modManager.mods()[idx];
    auto* cell = m_grid->getGridItemByIndex(idx);
    if (cell) static_cast<ModCard*>(cell)->setMod(mod.displayName, mod.type, mod.isInstalled, m_modManager.game().isModsDisabled, mod.modID);
    if (static_cast<size_t>(idx) == m_lastFocusIndex) m_tagType->setText(modTypeText(type));
}

void ModList::editModDescription() {
    int idx = m_focusedIndex.load();
    std::string desc = keyboard::showText(brls::getStr("modList/inputModDesc"), brls::getStr("modList/inputModDesc"), m_modManager.mods()[idx].description, 500);
    if (desc.empty()) return;
    m_modManager.setDescription(idx, desc);
    if (static_cast<size_t>(idx) == m_lastFocusIndex) m_descBody->setText(desc);
}

void ModList::editModVersion() {
    int idx = m_focusedIndex.load();
    std::string ver = keyboard::showNumber(brls::getStr("modList/inputModVersion"), m_modManager.mods()[idx].modVersion, 20);
    if (ver.empty()) return;
    m_modManager.setModVersion(idx, ver);
    updateDetail(idx);
}

void ModList::editGameVersion() {
    int idx = m_focusedIndex.load();
    std::string ver = keyboard::showNumber(brls::getStr("modList/inputGameVersion"), m_modManager.mods()[idx].gameVersion, 20);
    if (ver.empty()) return;
    m_modManager.setGameVersion(idx, ver);
    updateDetail(idx);
}

void ModList::editModAuthor() {
    int idx = m_focusedIndex.load();
    std::string author = keyboard::showText(brls::getStr("modList/inputModAuthor"), brls::getStr("modList/inputModAuthor"), m_modManager.mods()[idx].author, 34);
    if (author.empty()) return;
    m_modManager.setAuthor(idx, author);
    updateDetail(idx);
}

void ModList::setupEditMenu() {
    m_editMenu.title = brls::getStr("modList/editMenuTitle");

    auto& editNameItem = m_editMenu.addItem(brls::getStr("modList/editName"), brls::getStr("modList/editNameDesc"));
    editNameItem.setDisabled([this]{ return m_modManager.mods().empty(); });
    editNameItem.setStayOpen();
    editNameItem.setAction([this]{ manualSetModDisplayName(); });

    auto& restoreNameItem = m_editMenu.addItem(brls::getStr("modList/editRestoreName"), brls::getStr("modList/editRestoreNameDesc"));
    restoreNameItem.setDisabled([this]{ return m_modManager.mods().empty(); });
    restoreNameItem.setBadge([this]{
        if (m_modManager.mods().empty()) return std::string();
        return m_modManager.mods()[m_focusedIndex.load()].dirName;
    });
    restoreNameItem.setStayOpen();
    restoreNameItem.setAction([this]{ resetModDisplayName(); });

    auto& editTypeItem = m_editMenu.addItem(brls::getStr("modList/editType"), brls::getStr("modList/editTypeDesc"));
    editTypeItem.setDisabled([this]{ return m_modManager.mods().empty(); });
    editTypeItem.setBadge([this]{
        if (m_modManager.mods().empty()) return std::string();
        return modTypeText(m_modManager.mods()[m_focusedIndex.load()].type);
    });
    editTypeItem.setSubmenu(&m_typeMenu);

    auto& editDescItem = m_editMenu.addItem(brls::getStr("modList/editDesc"), brls::getStr("modList/editDescDesc"));
    editDescItem.setDisabled([this]{ return m_modManager.mods().empty(); });
    editDescItem.setStayOpen();
    editDescItem.setAction([this]{ editModDescription(); });

    auto& editModVerItem = m_editMenu.addItem(brls::getStr("modList/editModVer"), brls::getStr("modList/editModVerDesc"));
    editModVerItem.setDisabled([this]{ return m_modManager.mods().empty(); });
    editModVerItem.setBadge([this]{
        if (m_modManager.mods().empty()) return std::string();
        auto& v = m_modManager.mods()[m_focusedIndex.load()].modVersion;
        return v.empty() ? brls::getStr("modList/unknown") : v;
    });
    editModVerItem.setStayOpen();
    editModVerItem.setAction([this]{ editModVersion(); });

    auto& editGameVerItem = m_editMenu.addItem(brls::getStr("modList/editGameVer"), brls::getStr("modList/editGameVerDesc"));
    editGameVerItem.setDisabled([this]{ return m_modManager.mods().empty(); });
    editGameVerItem.setBadge([this]{
        if (m_modManager.mods().empty()) return std::string();
        auto& v = m_modManager.mods()[m_focusedIndex.load()].gameVersion;
        if (v.empty()) return brls::getStr("modList/unknown");
        if (v == "0") return brls::getStr("modList/versionUniversal");
        return v;
    });
    editGameVerItem.setStayOpen();
    editGameVerItem.setAction([this]{ editGameVersion(); });

    auto& editAuthorItem = m_editMenu.addItem(brls::getStr("modList/editAuthor"), brls::getStr("modList/editAuthorDesc"));
    editAuthorItem.setDisabled([this]{ return m_modManager.mods().empty(); });
    editAuthorItem.setBadge([this]{
        if (m_modManager.mods().empty()) return std::string();
        auto& v = m_modManager.mods()[m_focusedIndex.load()].author;
        return v.empty() ? brls::getStr("modList/unknown") : v;
    });
    editAuthorItem.setStayOpen();
    editAuthorItem.setAction([this]{ editModAuthor(); });

    // ── 类型选择子菜单 ──
    m_typeMenu.title = brls::getStr("modList/typeMenuTitle");

    for (auto& opt : modTypeOptions()) {
        std::string val = opt.value;
        auto& item = m_typeMenu.addItem(opt.label, opt.desc);
        item.setBadge([this, val]{
            if (m_modManager.mods().empty()) return std::string();
            return m_modManager.mods()[m_focusedIndex.load()].type == val ? std::string("\uE14B") : std::string();
        });
        item.setPopPage();
        item.setAction([this, val]{ applyModType(m_focusedIndex.load(), val); });
    }
}

void ModList::forceClean() {
    auto onConfirm = [this] {
        HomeBtnBlock::disable();
        ProgressDialog::show(brls::getStr("modList/forceCleanProgress"), {}, nullptr);

        auto alive = m_alive;

        m_installTask = util::async([this, alive](std::stop_token token) {
            auto result = m_modManager.forceClean(token, [alive](int deleted, int total, const char* fileName) {
                std::string fileNameStr = fileName ? fileName : "";
                bool scanning = (fileName == nullptr);
                brls::sync([=] {
                    if (!alive->load()) return;
                    if (scanning) {
                        ProgressDialog::setRightText("0 / " + std::to_string(total));
                    } else {
                        ProgressDialog::setLeftText(fileNameStr);
                        ProgressDialog::setRightText(std::to_string(deleted) + " / " + std::to_string(total));
                        ProgressDialog::setMainProgress(deleted * 100.0f / total);
                    }
                });
            });

            for (int i = 0; i < 30; i++) svcSleepThread(10000000ULL);  // 300ms 防闪烁

            brls::sync([this, alive, result] {
                HomeBtnBlock::enable();
                if (!alive->load()) return;

                std::string msg;
                switch (result.status) {
                    case fs::RemoveResult::Completed:
                        msg = brls::getStr("modList/forceCleanCompleted", result.elapsed);
                        m_gameManager.setModsDisabled(m_gameIndex, false);
                        m_gameManager.setHasInstalledMod(m_gameIndex, false);
                        break;
                    case fs::RemoveResult::FsError:
                        msg = brls::getStr("modList/forceCleanFailed", result.errorPath, result.errorCode);
                        break;
                    default: break;
                }
                CustomDialog::show(msg, {{brls::getStr("modList/ok"), [this] {
                    CustomDialog::close([this] {
                        m_modManager.sort(m_sortAsc);
                        refreshAndFocus(0);
                    });
                }}});
            });
        });
    };

    CustomDialog::show(brls::getStr("modList/forceCleanConfirm"), {
        {brls::getStr("modList/cancel"), [] { CustomDialog::close(); }},
        {brls::getStr("modList/cancel"), [] { CustomDialog::close(); }},
        {brls::getStr("modList/forceCleanBtn"), onConfirm},
    });
}

void ModList::setupManageMenu() {
    m_manageMenu.title = brls::getStr("modList/manageMenuTitle");

    auto& addModItem = m_manageMenu.addItem(brls::getStr("modList/addMod"), brls::getStr("modList/addModDesc"));
    addModItem.setBadge([]{ int n = ModManager::transitModCount(); return std::to_string(n) + " MOD"; });
    addModItem.setAction([this]{ addModsFromTransit(); });

    auto& removeModItem = m_manageMenu.addItem(brls::getStr("modList/removeMod"), brls::getStr("modList/removeModDesc"));
    removeModItem.setDisabled([this]{
        if (m_modManager.mods().size() <= 1) return true;
        auto idx = m_focusedIndex.load();
        if (idx >= static_cast<int>(m_modManager.mods().size())) return true;
        if (m_modManager.mods()[idx].isInstalled) return true;
        return m_sizeLoading.load();
    });
    removeModItem.setAction([this]{ removeModFromList(); });

    auto& disableItem = m_manageMenu.addItem(brls::getStr("modList/disableMod"), brls::getStr("modList/disableModDesc"));
    disableItem.setDisabled([this]{ return m_modManager.mods().empty(); });
    disableItem.setBadge([this]{ return m_modManager.game().isModsDisabled ? brls::getStr("modList/on") : brls::getStr("modList/off"); });
    disableItem.setBadgeHighlight([this]{ return m_modManager.game().isModsDisabled; });
    disableItem.setAction([this]{ toggleModDisable(); });

    auto& viewPathItem = m_manageMenu.addItem(brls::getStr("modList/viewPath"), brls::getStr("modList/viewPathDesc"));
    viewPathItem.setDisabled([this]{ return m_modManager.mods().empty(); });
    viewPathItem.setBadge([this]() {
        if (m_modManager.mods().empty()) return std::string();
        return m_modManager.mods()[m_focusedIndex.load()].dirName;
    });
    viewPathItem.setAction([this]{
        auto& mod = m_modManager.mods()[m_focusedIndex.load()];
        CustomDialog::show(mod.path, {{brls::getStr("modList/ok"), [] { CustomDialog::close(); }}});
    });

    auto& viewAuthorItem = m_manageMenu.addItem(brls::getStr("modList/viewAuthor"), brls::getStr("modList/viewAuthorDesc"));
    viewAuthorItem.setDisabled([this]{
        if (m_modManager.mods().empty()) return true;
        auto idx = m_focusedIndex.load();
        if (idx >= static_cast<int>(m_modManager.mods().size())) return true;
        return m_modManager.mods()[idx].authorLink.empty();
    });
    viewAuthorItem.setAction([this]{
        QrCodeView::show(m_modManager.mods()[m_focusedIndex.load()].authorLink);
    });

    // ── 子菜单：文件传输 ──
    m_transferMenu.title = brls::getStr("modList/transferMenuTitle");

    auto& mtpItem = m_transferMenu.addItem(brls::getStr("modList/mtpTransfer"), brls::getStr("modList/mtpTransferDesc"));
    mtpItem.setAction([] {
        std::vector<MtpMount> mounts = {
            {"/mods2/!temp_mods", "Add Mod"},
        };
        mtpDialog::open(mounts);
    });

    auto& ftpItem = m_transferMenu.addItem(brls::getStr("modList/ftpTransfer"), brls::getStr("modList/ftpTransferDesc"));
    ftpItem.setAction([] {
        std::vector<FtpMount> mounts = {
            {"/mods2/!temp_mods", "Add Mod"},
        };
        ftpDialog::open(mounts);
    });
}

void ModList::addModsFromTransit() {
    auto mods = ModManager::scanTransitMods();
    if (mods.empty()) {
        CustomDialog::show(brls::getStr("modList/transitEmpty"), {{brls::getStr("modList/ok"), [] { CustomDialog::close(); }}});
        return;
    }

    auto* menu = new MenuPageConfig();
    menu->title = brls::getStr("modList/selectMod");
    menu->multiSelect = true;
    for (const auto& mod : mods){
        menu->addItem(mod.name);
    }
        
    menu->onConfirm = [this, mods = std::move(mods)](const std::vector<int>& selected) mutable {
        CustomDialog::show(brls::getStr("modList/addModConfirm", std::to_string(selected.size())), {
            {brls::getStr("modList/cancel"), [] { CustomDialog::close(); }},
            {brls::getStr("modList/confirm"), [this, mods = std::move(mods), selected] {
                std::vector<fs::DirEntry> chosen;
                for (int i : selected) chosen.push_back(mods[i]);

                int added = m_modManager.addModsFormTransitForModList(chosen);
                std::string lastDir = m_modManager.mods().back().dirName;
                m_modManager.sort(m_sortAsc);
                int newIdx = std::max(0, m_modManager.findByDirName(lastDir));

                auto& game = m_gameManager.games()[m_gameIndex];
                game.modCount = std::to_string(m_modManager.mods().size());
                m_gameManager.setPendingFocus(game.appId);

                CustomDialog::show(brls::getStr("modList/addModSuccess", std::to_string(added)), {{brls::getStr("modList/ok"), [this, newIdx] {
                    CustomDialog::close([this, newIdx] {
                        refreshAndFocus(newIdx);
                        startSizeLoader();
                    });
                }}});
            }},
        });
    };

    menu->show();
}

void ModList::removeModFromList() {
    int idx = m_focusedIndex.load();
    auto& mod = m_modManager.mods()[idx];

    auto onConfirm = [this, idx] {
        m_modManager.removeModFromModList(idx);

        auto& game = m_gameManager.games()[m_gameIndex];
        game.modCount = std::to_string(m_modManager.mods().size());
        m_gameManager.setPendingFocus(game.appId);

        if (m_modManager.mods().empty()) {
            auto goBack = [this] {
                CustomDialog::close([this] {
                    auto alive = m_alive;
                    brls::sync([alive] {
                        if (!alive->load()) return;
                        brls::Application::popActivity();
                    });
                });
            };
            CustomDialog::show(brls::getStr("modList/noModLeft"), {{brls::getStr("modList/ok"), goBack}}, goBack);
            return;
        }

        int newFocus = std::min(idx, static_cast<int>(m_modManager.mods().size()) - 1);
        CustomDialog::close([this, newFocus] { refreshAndFocus(newFocus); });
    };

    CustomDialog::show(brls::getStr("modList/removeModConfirm"), {
        {brls::getStr("modList/cancel"), [] { CustomDialog::close(); }},
        {brls::getStr("modList/cancel"), [] { CustomDialog::close(); }},
        {brls::getStr("modList/removeModBtn"), onConfirm},
    });
}

void ModList::setupMenu() {
    setupEditMenu();
    setupManageMenu();

    // ── 主菜单 ──
    m_menu.title = brls::getStr("modList/mainMenuTitle");

    m_menu.shouldShowFakeHighlight = [this]{ return !m_modManager.mods().empty(); };

    auto& editSubmenuItem = m_menu.addItem(brls::getStr("modList/editSubmenu"), brls::getStr("modList/editSubmenuDesc"));
    editSubmenuItem.setDisabled([this]{ return m_modManager.mods().empty(); });
    editSubmenuItem.setBadge("\uE14A");
    editSubmenuItem.setSubmenu(&m_editMenu);

    auto& manageSubmenuItem = m_menu.addItem(brls::getStr("modList/manageSubmenu"), brls::getStr("modList/manageSubmenuDesc"));
    manageSubmenuItem.setBadge("\uE14A");
    manageSubmenuItem.setSubmenu(&m_manageMenu);

    auto& transferSubmenuItem = m_menu.addItem(brls::getStr("modList/transferSubmenu"), brls::getStr("modList/transferSubmenuDesc"));
    transferSubmenuItem.setBadge("\uE14A");
    transferSubmenuItem.setSubmenu(&m_transferMenu);

    auto& forceCleanItem = m_menu.addItem(brls::getStr("modList/forceClean"), brls::getStr("modList/forceCleanDesc"));
    forceCleanItem.setDisabled([this]{ return m_modManager.mods().empty(); });
    forceCleanItem.setBadge("\uE14A");
    forceCleanItem.setAction([this]{ forceClean(); });
}

void ModList::setupStoreMenu() {
    m_storeMenu.title = brls::getStr("modList/storeMenuTitle");

    auto& modListItem = m_storeMenu.addItem(brls::getStr("modList/storeModList"), brls::getStr("modList/storeModListDesc"));
    modListItem.setBadge("\uE14A");
    modListItem.setAction([this] {
        auto& game = m_gameManager.games()[m_gameIndex];
        std::string tid = format::appIdHex(game.appId);
        pageNav::push(new StoreModList(tid, game.displayName, m_gameManager, &m_modManager));
    });

    auto& modDetailItem = m_storeMenu.addItem(brls::getStr("modList/storeModDetail"), brls::getStr("modList/storeModDetailDesc"));
    modDetailItem.setDisabled([this] {
        if (m_modManager.mods().empty()) return true;
        auto idx = m_focusedIndex.load();
        if (idx >= static_cast<int>(m_modManager.mods().size())) return true;
        return m_modManager.mods()[idx].modID < 0;
    });
    modDetailItem.setBadge("\uE14A");
    modDetailItem.setAction([this] {
        auto& mod = m_modManager.mods()[m_focusedIndex.load()];
        auto& game = m_gameManager.games()[m_gameIndex];
        std::string tid = format::appIdHex(game.appId);
        pageNav::push(new StoreModDetail(mod.modID, tid, game.displayName, m_gameManager, nullptr, &m_modManager));
    });

    auto& uploadItem = m_storeMenu.addItem(brls::getStr("modList/uploadMod"), brls::getStr("modList/uploadModDesc"));
    uploadItem.setBadge("\uE14A");
    uploadItem.setAction([] {
        QrCodeView::show(config::websiteUrl);
    });

    auto& nicknameItem = m_storeMenu.addItem(brls::getStr("modList/nickname"), brls::getStr("modList/nicknameDesc"));
    nicknameItem.setBadge([]{ return Settings::getString("modShop", "nickname", brls::getStr("modList/anonymousUser")); });
    nicknameItem.setAction([this]{ setNickname(); });
    nicknameItem.setStayOpen();
}

void ModList::setNickname() {
    std::string current = Settings::getString("modShop", "nickname");
    std::string name = keyboard::showText(brls::getStr("modList/inputNickname"), brls::getStr("modList/inputNickname"), current, 32);
    if (name.empty()) return;
    Settings::setString("modShop", "nickname", name);
}