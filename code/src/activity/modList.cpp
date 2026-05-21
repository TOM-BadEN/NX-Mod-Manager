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
#include "view/longPressDialog.hpp"
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
    m_stopSource.request_stop();
    m_installStop.request_stop();
}

void ModList::onResume() {
    int modID = m_modManager.consumePendingFocus();
    if (modID < 0) return;
    int newIdx = m_modManager.findByModID(modID);
    if (newIdx >= 0) {
        m_grid->deferReload(newIdx);
        m_focusedIndex = newIdx;
    }
}

void ModList::onContentAvailable() {
    m_frame->setTitle(m_modManager.game().displayName);
    if (m_modManager.game().version != "...") m_frame->setSubtitle(format::cleanVersion(m_modManager.game().version));
    setupDetail();
    setupModGrid();

    if (!m_modManager.mods().empty()) updateDetail(0);

    // ZL：进入商店页面
    m_frame->registerAction(brls::getStr("modList/zlStore"), brls::BUTTON_LT, [this](...) {
        Audio::instance()->play(SoundEffect::Enter);
        auto& game = m_gameManager.games()[m_gameIndex];
        std::string tid = format::appIdHex(game.appId);
        pageNav::push(new StoreModList(tid, game.displayName, m_gameManager, &m_modManager, game.version), pageNav::Anim::SLIDE_LEFT);
        return true;
    });

    // ZR：本地添加菜单
    m_frame->registerAction(brls::getStr("modList/zrTransfer"), brls::BUTTON_RT, [this](...) {
        Audio::instance()->play(SoundEffect::Enter);
        m_localAddMenu.show();
        return true;
    });

    m_frame->registerAction(brls::getStr("modList/sortAsc"), brls::BUTTON_Y, [this](...) {
        Audio::instance()->play(SoundEffect::Click);
        toggleSort();
        return true;
    });

    setupMenu();

    m_frame->registerAction(brls::getStr("modList/menu"), brls::BUTTON_X, [this](...) {
        Audio::instance()->play(SoundEffect::Enter);
        m_menu.show();
        return true;
    });

    submitNextSize();
    runFirstLaunchDialog();
}

void ModList::toggleModInstall(int index) {
    auto& mod = m_modManager.mods()[index];
    bool installing = !mod.isInstalled;
    std::string modName = mod.displayName;
    std::string dirName = mod.dirName;

    auto onConfirm = [this, index, installing, modName, dirName] {
        HomeBtnBlock::disable();
        m_installStop = std::stop_source{};
        auto installToken = m_installStop.get_token();
        auto pageToken = m_stopSource.get_token();

        std::string title = installing ? brls::getStr("modList/installing", modName) : brls::getStr("modList/uninstalling", modName);
        if (installing) {
            auto onCancel = [this] { m_installStop.request_stop(); };
            ProgressDialog::show(title, {{brls::getStr("modList/cancel"), onCancel}}, onCancel);
        } else {
            ProgressDialog::show(title, {}, [] {});
        }

        auto cleaningStarted = std::make_shared<bool>(false);

        auto progressCb = [installToken, cleaningStarted, modName](const ModInstaller::Progress& progress) {
            bool cleaning    = progress.cleaning;
            int current      = progress.current;
            int total        = progress.total;
            std::string file = progress.currentFile;
            int64_t written  = progress.bytesWritten;
            int64_t fileSize = progress.bytesTotal;

            brls::sync([=] {
                if (installToken.stop_requested()) return;

                if (cleaning && !*cleaningStarted) {
                    *cleaningStarted = true;
                    Audio::instance()->play(SoundEffect::Warning);
                    ProgressDialog::setTitle(brls::getStr("modList/cleaning", modName));
                    ProgressDialog::setMainProgressColor(brls::Application::getTheme().getColor("app/textWarning"));
                    ProgressDialog::hideButtons();
                }

                ProgressDialog::setLeftText(file);
                if (total > 0) ProgressDialog::setRightText(std::to_string(current) + " / " + std::to_string(total));
                if (current > 0 && total > 0) ProgressDialog::setMainProgress(current * 100.0f / total);
                if (!*cleaningStarted && written > 0 && fileSize > 0) ProgressDialog::setSubProgress(written, fileSize);
                else ProgressDialog::hideSubProgress();
            });
        };

        m_installTask = ThreadPool::instance().submitWaitable([this, index, installing, dirName, progressCb, pageToken](std::stop_token token) {
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
            brls::sync([this, installing, success, cancelled, errorMsg = std::move(errorMsg), errorFile = std::move(errorFile), conflictMod = std::move(conflictMod), dirName, elapsed, pageToken] {
                HomeBtnBlock::enable();
                if (pageToken.stop_requested()) return;

                if (success) {
                    m_gameManager.setHasInstalledMod(m_gameIndex, installing || ModManager::hasInstalledMod(m_modManager.game().dirPath));
                    std::string msg = installing ? brls::getStr("modList/installSuccess", elapsed) : brls::getStr("modList/uninstallSuccess", elapsed);
                    m_modManager.sort();
                    int focusIdx = std::max(0, m_modManager.findByDirName(dirName));
                    auto onClose = [this, focusIdx] { CustomDialog::close([this, focusIdx] { refreshAndFocus(focusIdx); }); };
                    CustomDialog::show(msg, {{brls::getStr("modList/ok"), onClose}}, onClose);
                    return;
                }

                std::string msg;
                if (cancelled) msg = brls::getStr("modList/installCancelled");
                else if (!conflictMod.empty()) msg = brls::getStr("modList/installConflict", conflictMod, errorFile);
                else msg = (installing ? brls::getStr("modList/installFailed", errorMsg, errorFile) : brls::getStr("modList/uninstallFailed", errorMsg, errorFile));
                CustomDialog::show(msg, {{brls::getStr("modList/ok"), [] { CustomDialog::close(); }}});
            });
        }, installToken);
    };

    std::string confirmMsg = installing ? brls::getStr("modList/confirmInstall") : brls::getStr("modList/confirmUninstall");

    CustomDialog::show(confirmMsg, {
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

        m_installStop = std::stop_source{};
        auto installToken = m_installStop.get_token();
        auto pageToken = m_stopSource.get_token();
        m_installTask = ThreadPool::instance().submitWaitable([this, disabling, pageToken](std::stop_token) {
            bool ok;
            if (disabling) ok = m_modManager.disableMods();
            else ok = m_modManager.enableMods();

            brls::sync([this, disabling, ok, pageToken] {
                if (pageToken.stop_requested()) return;
                if (!ok) {
                    CustomDialog::show(brls::getStr("modList/disableFailed"), {{brls::getStr("modList/ok"), [] { CustomDialog::close(); }}});
                    return;
                }
                if (!disabling) m_gameManager.setModsDisabled(m_gameIndex, false);
                std::string doneMsg = disabling ? brls::getStr("modList/disableSuccess") : brls::getStr("modList/enableSuccess");
                auto onClose = [this] { CustomDialog::close([this] { refreshAndFocus(m_focusedIndex); }); };
                CustomDialog::show(doneMsg, {{brls::getStr("modList/ok"), onClose}}, onClose);
            });
        }, installToken);
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
        m_focusedIndex = index;
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
    m_modManager.toggleSortAsc();
    refreshAndFocus(0);
    m_frame->updateActionHint(brls::BUTTON_Y, m_modManager.sortAsc() ? brls::getStr("modList/sortAsc") : brls::getStr("modList/sortDesc"));
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
    m_tagVersion->setText(mod.modVersion.empty() ? brls::getStr("modList/modVersionUnknown") : brls::getStr("modList/modVersionFmt", format::cleanVersion(mod.modVersion)));
    if (mod.gameVersion.empty()) m_tagGameVer->setText(brls::getStr("modList/gameVersionUnknown"));
    else if (mod.gameVersion == "0") m_tagGameVer->setText(brls::getStr("modList/gameVersionUniversal"));
    else m_tagGameVer->setText(brls::getStr("modList/gameVersionFmt", format::cleanVersion(mod.gameVersion)));
    m_descBody->setText(mod.description.empty() ? brls::getStr("modList/noDescription") : mod.description);
}

void ModList::startIconLoader() {
    auto token = m_stopSource.get_token();
    std::string tid = format::appIdHex(m_modManager.game().appId);
    ThreadPool::instance().submit([this, tid](std::stop_token token) {
        for (int retry = 0; retry < 10 && !token.stop_requested(); retry++) {
            for (int i = 0; i < 100 && !token.stop_requested(); i++) svcSleepThread(10000000ULL);  // 10ms × 100 = 1s
            if (token.stop_requested()) break;
            int iconId = brls::TextureCache::instance().getCache(tid);
            if (iconId > 0) {
                brls::sync([this, iconId, token]() {
                    if (token.stop_requested()) return;
                    m_gameIcon->innerSetImage(iconId);
                });
                break;
            }
        }
    }, token);
}

void ModList::submitNextSize() {
    m_sizeLoading = true;
    auto token = m_stopSource.get_token();
    auto& mods = m_modManager.mods();

    // 主线程找离焦点最近的未计算 mod
    int center = m_focusedIndex;
    int bestIdx = -1;
    int bestDist = INT_MAX;
    for (int i = 0; i < static_cast<int>(mods.size()); i++) {
        if (mods[i].size.empty()) {
            int dist = std::abs(i - center);
            if (dist < bestDist) { bestDist = dist; bestIdx = i; }
        }
    }

    if (bestIdx < 0) {
        m_sizeLoading = false;
        m_modManager.saveJson();
        return;
    }

    int modIdx = bestIdx;
    std::string modPath = mods[modIdx].path;

    ThreadPool::instance().submit([this, modIdx, modPath](std::stop_token token) {
        int64_t bytes = fs::calcDirSize(modPath, &token);
        if (token.stop_requested()) return;
        std::string sizeStr = (bytes >= 0) ? format::fileSize(bytes) : "";

        brls::sync([this, modIdx, sizeStr = std::move(sizeStr), token]() {
            if (token.stop_requested()) return;
            if (!sizeStr.empty()) applySizeResult(modIdx, sizeStr);
            submitNextSize();
        });
    }, token);
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
    int idx = m_focusedIndex;
    std::string name = keyboard::showText(brls::getStr("modList/inputModName"), brls::getStr("modList/inputModName"), m_modManager.mods()[idx].displayName, 64);
    if (name.empty()) return;
    applyModDisplayName(idx, name);
}

void ModList::resetModDisplayName() {
    int idx = m_focusedIndex;
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
    int idx = m_focusedIndex;
    std::string desc = keyboard::showText(brls::getStr("modList/inputModDesc"), brls::getStr("modList/inputModDesc"), m_modManager.mods()[idx].description, 500);
    if (desc.empty()) return;
    m_modManager.setDescription(idx, desc);
    if (static_cast<size_t>(idx) == m_lastFocusIndex) m_descBody->setText(desc);
}

void ModList::editModVersion() {
    int idx = m_focusedIndex;
    std::string ver = keyboard::showNumber(brls::getStr("modList/inputModVersion"), m_modManager.mods()[idx].modVersion, 20);
    if (ver.empty()) return;
    m_modManager.setModVersion(idx, ver);
    updateDetail(idx);
}

void ModList::editGameVersion() {
    int idx = m_focusedIndex;
    std::string ver = keyboard::showNumber(brls::getStr("modList/inputGameVersion"), m_modManager.mods()[idx].gameVersion, 20);
    if (ver.empty()) return;
    m_modManager.setGameVersion(idx, ver);
    updateDetail(idx);
}

void ModList::editModAuthor() {
    int idx = m_focusedIndex;
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
        return m_modManager.mods()[m_focusedIndex].dirName;
    });
    restoreNameItem.setStayOpen();
    restoreNameItem.setAction([this]{ resetModDisplayName(); });

    auto& editTypeItem = m_editMenu.addItem(brls::getStr("modList/editType"), brls::getStr("modList/editTypeDesc"));
    editTypeItem.setDisabled([this]{ return m_modManager.mods().empty(); });
    editTypeItem.setBadge([this]{
        if (m_modManager.mods().empty()) return std::string();
        return modTypeText(m_modManager.mods()[m_focusedIndex].type);
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
        auto& v = m_modManager.mods()[m_focusedIndex].modVersion;
        return v.empty() ? brls::getStr("modList/unknown") : v;
    });
    editModVerItem.setStayOpen();
    editModVerItem.setAction([this]{ editModVersion(); });

    auto& editGameVerItem = m_editMenu.addItem(brls::getStr("modList/editGameVer"), brls::getStr("modList/editGameVerDesc"));
    editGameVerItem.setDisabled([this]{ return m_modManager.mods().empty(); });
    editGameVerItem.setBadge([this]{
        if (m_modManager.mods().empty()) return std::string();
        auto& v = m_modManager.mods()[m_focusedIndex].gameVersion;
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
        auto& v = m_modManager.mods()[m_focusedIndex].author;
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
            return m_modManager.mods()[m_focusedIndex].type == val ? std::string("\uE14B") : std::string();
        });
        item.setPopPage();
        item.setAction([this, val]{ applyModType(m_focusedIndex, val); });
    }
}

void ModList::forceClean() {
    auto onConfirm = [this] {
        HomeBtnBlock::disable();
        ProgressDialog::show(brls::getStr("modList/forceCleanProgress"), {}, nullptr);

        m_installStop = std::stop_source{};
        auto installToken = m_installStop.get_token();
        auto pageToken = m_stopSource.get_token();

        m_installTask = ThreadPool::instance().submitWaitable([this, pageToken](std::stop_token token) {
            auto result = m_modManager.forceClean(token, [pageToken](int deleted, int total, const char* fileName) {
                std::string fileNameStr = fileName ? fileName : "";
                bool scanning = (fileName == nullptr);
                brls::sync([=] {
                    if (pageToken.stop_requested()) return;
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

            brls::sync([this, result, pageToken] {
                HomeBtnBlock::enable();
                if (pageToken.stop_requested()) return;

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
                auto onClose = [this] { CustomDialog::close([this] { m_modManager.sort(); refreshAndFocus(0); }); };
                CustomDialog::show(msg, {{brls::getStr("modList/ok"), onClose}}, onClose);
            });
        }, installToken);
    };

    CustomDialog::show(brls::getStr("modList/forceCleanConfirm"), {
        {brls::getStr("modList/cancel"), [] { CustomDialog::close(); }},
        {brls::getStr("modList/cancel"), [] { CustomDialog::close(); }},
        {brls::getStr("modList/forceCleanBtn"), onConfirm},
    });
}

void ModList::setupManageMenu() {
    m_manageMenu.title = brls::getStr("modList/manageMenuTitle");

    auto& editSubmenuItem = m_manageMenu.addItem(brls::getStr("modList/editSubmenu"), brls::getStr("modList/editSubmenuDesc"));
    editSubmenuItem.setDisabled([this]{ return m_modManager.mods().empty(); });
    editSubmenuItem.setBadge("\uE14A");
    editSubmenuItem.setSubmenu(&m_editMenu);

    auto& removeModItem = m_manageMenu.addItem(brls::getStr("modList/removeMod"), brls::getStr("modList/removeModDesc"));
    removeModItem.setDisabled([this]{
        if (m_modManager.mods().empty()) return true;
        auto idx = m_focusedIndex;
        if (idx >= static_cast<int>(m_modManager.mods().size())) return true;
        if (m_modManager.mods()[idx].isInstalled) return true;
        return m_sizeLoading;
    });
    removeModItem.setAction([this]{
        if (m_modManager.mods().size() == 1) removeLastModFromList();
        else removeModFromList();
    });

    auto& viewPathItem = m_manageMenu.addItem(brls::getStr("modList/viewPath"), brls::getStr("modList/viewPathDesc"));
    viewPathItem.setDisabled([this]{ return m_modManager.mods().empty(); });
    viewPathItem.setBadge([this]() {
        if (m_modManager.mods().empty()) return std::string();
        return m_modManager.mods()[m_focusedIndex].dirName;
    });
    viewPathItem.setAction([this]{
        auto& mod = m_modManager.mods()[m_focusedIndex];
        CustomDialog::show(mod.path, {{brls::getStr("modList/ok"), [] { CustomDialog::close(); }}});
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
                m_modManager.sort();
                int newIdx = std::max(0, m_modManager.findByDirName(lastDir));

                auto& game = m_gameManager.games()[m_gameIndex];
                game.modCount = std::to_string(m_modManager.mods().size());
                m_gameManager.setPendingFocus(game.appId);

                auto onClose = [this, newIdx] { CustomDialog::close([this, newIdx] { refreshAndFocus(newIdx); submitNextSize(); }); };
                CustomDialog::show(brls::getStr("modList/addModSuccess", std::to_string(added)), {{brls::getStr("modList/ok"), onClose}}, onClose);
            }},
        });
    };

    menu->show();
}

void ModList::removeModFromList() {
    int idx = m_focusedIndex;

    auto onConfirm = [this, idx] {
        m_modManager.removeModFromModList(idx);

        auto& game = m_gameManager.games()[m_gameIndex];
        game.modCount = std::to_string(m_modManager.mods().size());
        m_gameManager.setPendingFocus(game.appId);

        int newFocus = std::min(idx, static_cast<int>(m_modManager.mods().size()) - 1);
        CustomDialog::close([this, newFocus] { refreshAndFocus(newFocus); });
    };

    CustomDialog::show(brls::getStr("modList/removeModConfirm"), {
        {brls::getStr("modList/cancel"), [] { CustomDialog::close(); }},
        {brls::getStr("modList/cancel"), [] { CustomDialog::close(); }},
        {brls::getStr("modList/removeModBtn"), onConfirm},
    });
}

void ModList::removeLastModFromList() {
    auto onConfirm = [this] {
        auto& game = m_gameManager.games()[m_gameIndex];
        m_gameManager.setPendingRemove(game.appId);
        CustomDialog::close([] {
            brls::Application::popActivity();
        });
    };

    CustomDialog::show(brls::getStr("modList/removeLastModConfirm"), {
        {brls::getStr("modList/cancel"), [] { CustomDialog::close(); }},
        {brls::getStr("modList/cancel"), [] { CustomDialog::close(); }},
        {brls::getStr("modList/removeModBtn"), onConfirm},
    });
}

void ModList::setupMenu() {
    setupEditMenu();
    setupStoreAddMenu();
    setupLocalAddMenu();
    setupAddProjectMenu();
    setupViewModMenu();
    setupManageMenu();

    // ── 主菜单 ──
    m_menu.title = brls::getStr("modList/mainMenuTitle");

    m_menu.shouldShowFakeHighlight = [this]{ return !m_modManager.mods().empty(); };

    auto& addProjectItem = m_menu.addItem(brls::getStr("modList/addProjectSubmenu"), brls::getStr("modList/addProjectSubmenuDesc"));
    addProjectItem.setBadge("\uE14A");
    addProjectItem.setSubmenu(&m_addProjectMenu);

    auto& viewModItem = m_menu.addItem(brls::getStr("modList/viewModSubmenu"), brls::getStr("modList/viewModSubmenuDesc"));
    viewModItem.setDisabled([this] {
        if (m_modManager.mods().empty()) return true;
        auto idx = m_focusedIndex;
        if (idx >= static_cast<int>(m_modManager.mods().size())) return true;
        return m_modManager.mods()[idx].modID < 0;
    });
    viewModItem.setBadge("\uE14A");
    viewModItem.setSubmenu(&m_viewModMenu);

    auto& manageSubmenuItem = m_menu.addItem(brls::getStr("modList/manageSubmenu"), brls::getStr("modList/manageSubmenuDesc"));
    manageSubmenuItem.setBadge("\uE14A");
    manageSubmenuItem.setSubmenu(&m_manageMenu);

    auto& disableItem = m_menu.addItem(brls::getStr("modList/disableMod"), brls::getStr("modList/disableModDesc"));
    disableItem.setDisabled([this]{ return m_modManager.mods().empty(); });
    disableItem.setBadge([this]{ return m_modManager.game().isModsDisabled ? brls::getStr("modList/on") : brls::getStr("modList/off"); });
    disableItem.setBadgeHighlight([this]{ return m_modManager.game().isModsDisabled; });
    disableItem.setAction([this]{ toggleModDisable(); });

    auto& forceCleanItem = m_menu.addItem(brls::getStr("modList/forceClean"), brls::getStr("modList/forceCleanDesc"));
    forceCleanItem.setDisabled([this]{ return m_modManager.mods().empty(); });
    forceCleanItem.setAction([this]{ forceClean(); });
}

void ModList::setupViewModMenu() {
    m_viewModMenu.title = brls::getStr("modList/viewModSubmenu");

    auto& modDetailItem = m_viewModMenu.addItem(brls::getStr("modList/storeModDetail"), brls::getStr("modList/storeModDetailDesc"));
    modDetailItem.setBadge("\uE14A");
    modDetailItem.setAction([this] {
        auto& mod = m_modManager.mods()[m_focusedIndex];
        auto& game = m_gameManager.games()[m_gameIndex];
        std::string tid = format::appIdHex(game.appId);
        pageNav::push(new StoreModDetail(mod.modID, tid, game.displayName, m_gameManager, nullptr, &m_modManager, game.version, mod.modID > 0), pageNav::Anim::SLIDE_LEFT);
    });

    auto& viewAuthorItem = m_viewModMenu.addItem(brls::getStr("modList/viewAuthor"), brls::getStr("modList/viewAuthorDesc"));
    viewAuthorItem.setBadge("\uE14A");
    viewAuthorItem.setDisabled([this]{
        if (m_modManager.mods().empty()) return true;
        auto idx = m_focusedIndex;
        if (idx >= static_cast<int>(m_modManager.mods().size())) return true;
        return m_modManager.mods()[idx].authorLink.empty();
    });
    viewAuthorItem.setAction([this]{
        QrCodeView::show(m_modManager.mods()[m_focusedIndex].authorLink);
    });
}

void ModList::setupLocalAddMenu() {
    m_localAddMenu.title = brls::getStr("modList/localAdd");

    auto& selectModItem = m_localAddMenu.addItem(brls::getStr("modList/selectMod"), brls::getStr("modList/selectModDesc"));
    selectModItem.setBadge([]{ int n = ModManager::transitModCount(); return std::to_string(n) + " MOD"; });
    selectModItem.setAction([this]{ addModsFromTransit(); });

    auto& mtpItem = m_localAddMenu.addItem(brls::getStr("modList/mtpTransfer"), brls::getStr("modList/mtpTransferDesc"));
    mtpItem.setAction([] {
        std::vector<MtpMount> mounts = {
            {"/mods2/!temp_mods", "Add Mod"},
        };
        mtpDialog::open(mounts);
    });

    auto& ftpItem = m_localAddMenu.addItem(brls::getStr("modList/ftpTransfer"), brls::getStr("modList/ftpTransferDesc"));
    ftpItem.setAction([] {
        std::vector<FtpMount> mounts = {
            {"/mods2/!temp_mods", "Add Mod"},
        };
        ftpDialog::open(mounts);
    });
}

void ModList::setupAddProjectMenu() {
    m_addProjectMenu.title = brls::getStr("modList/addProjectSubmenu");

    auto& storeAddItem = m_addProjectMenu.addItem(brls::getStr("modList/storeAddSubmenu"), brls::getStr("modList/storeAddSubmenuDesc"));
    storeAddItem.setBadge("\uE14A");
    storeAddItem.setSubmenu(&m_storeAddMenu);

    auto& localAddItem = m_addProjectMenu.addItem(brls::getStr("modList/localAdd"), brls::getStr("modList/localAddDesc"));
    localAddItem.setBadge("\uE14A");
    localAddItem.setSubmenu(&m_localAddMenu);
}

void ModList::setupStoreAddMenu() {
    m_storeAddMenu.title = brls::getStr("modList/storeAddSubmenu");

    auto& modListItem = m_storeAddMenu.addItem(brls::getStr("modList/storeModList"), brls::getStr("modList/storeModListDesc"));
    modListItem.setBadge("\uE14A");
    modListItem.setAction([this] {
        auto& game = m_gameManager.games()[m_gameIndex];
        std::string tid = format::appIdHex(game.appId);
        pageNav::push(new StoreModList(tid, game.displayName, m_gameManager, &m_modManager, game.version), pageNav::Anim::SLIDE_LEFT);
    });

    auto& uploadItem = m_storeAddMenu.addItem(brls::getStr("modList/uploadMod"), brls::getStr("modList/uploadModDesc"));
    uploadItem.setBadge("\uE14A");
    uploadItem.setAction([] {
        QrCodeView::show(config::websiteUrl);
    });
    uploadItem.setStayOpen();

    auto& nicknameItem = m_storeAddMenu.addItem(brls::getStr("modList/nickname"), brls::getStr("modList/nicknameDesc"));
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

void ModList::runFirstLaunchDialog() {
    if (!Settings::getBool("modList", "firstLaunch", true)) return;
    brls::sync([this]() {
        auto onConfirm = []() {
            Settings::setBool("modList", "firstLaunch", false);
            LongPressDialog::close();
        };
        LongPressDialog::show(brls::getStr("modList/firstLaunchNotice"), brls::getStr("modList/firstLaunchBtn"), 5.0f, onConfirm);
    });
}