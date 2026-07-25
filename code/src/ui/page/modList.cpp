/**
 * ModList - Mod 列表页面
 * 左侧 RecyclingGrid（单列 Mod 列表） + 右侧 Mod 详情
 */

#include "ui/page/modList.hpp"
#include <borealis/core/i18n.hpp>
#include "core/audio.hpp"
#include "core/modInstaller/dontStarve.hpp"
#include "core/modInstaller/mhrise.hpp"
#include "ui/view/modCard.hpp"
#include "ui/dataSource/modCardDS.hpp"
#include "utils/format.hpp"
#include "core/device.hpp"
#include "utils/fsHelper.hpp"
#include "ui/view/dialog/customDialog.hpp"
#include "ui/view/dialog/progressDialog.hpp"
#include "ui/view/dialog/mtpDialog.hpp"
#include "ui/view/dialog/ftpDialog.hpp"
#include "ui/view/qrCodeView.hpp"
#include "ui/page/storeModList.hpp"
#include "ui/page/storeModDetail.hpp"
#include "common/settings.hpp"
#include "ui/view/dialog/longPressDialog.hpp"
#include "ui/view/longTextBox.hpp"
#include "ui/view/dialog/scrollDialog.hpp"
#include "common/config.hpp"
#include "utils/keyboard.hpp"
// #include "utils/pageNav.hpp"
#include "utils/crc32.hpp"
#include <borealis/core/cache_helper.hpp>
#include <chrono>
#include <yoga/Yoga.h>
#include <switch.h>

ModList::ModList(size_t gameIndex, GameManager& gameManager)
    : m_gameManager(gameManager), m_gameIndex(gameIndex), m_modManager(gameManager.games()[gameIndex]) {
    inflateFromXMLRes("xml/view/page/modList.xml");
}

ModList::~ModList() {
    m_stopSource.request_stop();
    m_installStop.request_stop();
    m_installTask.wait();
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
    ShellState::setTitle(m_modManager.game().displayName);
    ShellState::setSubtitle(brls::getStr("page/modList/notInstalled"));
    if (m_modManager.game().version != "...") ShellState::setSubtitle(format::cleanVersion(m_modManager.game().version));
    setupDetail();
    setupModGrid();

    if (!m_modManager.mods().empty()) updateDetail(0);

    // ZL：进入商店页面
    registerAction(brls::getStr("page/modList/zlStore"), brls::BUTTON_LT, [this](...) {
        Audio::instance()->play(SoundEffect::Enter);
        auto& game = m_gameManager.games()[m_gameIndex];
        std::string tid = format::appIdHex(game.appId);
        Page::pushPage(new StoreModList(tid, game.displayName, m_gameManager, &m_modManager, game.version, true), PageAnimType::SlideFromLeft);
        return true;
    });

    // ZR：本地添加菜单
    registerAction(brls::getStr("page/modList/zrTransfer"), brls::BUTTON_RT, [this](...) {
        Audio::instance()->play(SoundEffect::Enter);
        m_localAddMenu.show();
        return true;
    });

    registerAction(brls::getStr("page/modList/sortAsc"), brls::BUTTON_Y, [this](...) {
        Audio::instance()->play(SoundEffect::Click);
        toggleSort();
        return true;
    });

    setupMenu();

    registerAction(brls::getStr("page/modList/menu"), brls::BUTTON_X, [this](...) {
        Audio::instance()->play(SoundEffect::Enter);
        m_menu.show();
        return true;
    });

    if (!m_modManager.hasUnmanagedMods()) {
        submitNextSize();
        submitNextCrc();
    }
    runFirstLaunchDialog();
}

bool ModList::checkBeforeModInstall(int index) {
    switch (m_modManager.modGameType()) {
        case ModGameType::MHRise:
            return checkMHRiseRules();
        case ModGameType::DontStarve:
            return checkDontStarveRules(index);
        case ModGameType::Normal:
        default:
            return true;
    }
}

bool ModList::checkMHRiseRules() {
    const auto& game = m_modManager.game();
    bool gameInstalled = game.isInstalled;

    auto result = ModInstaller::mhrise::checkBeforeInstall(game.version, gameInstalled, game.dirPath);
    if (result == ModInstaller::mhrise::InstallPrecheckResult::Ok) return true;

    std::string msg;
    if (result == ModInstaller::mhrise::InstallPrecheckResult::GameNotInstalled) {
        msg = brls::getStr("page/modList/mhriseGameNotInstalled");
    } else if (result == ModInstaller::mhrise::InstallPrecheckResult::VersionChanged) {
        msg = brls::getStr("page/modList/mhriseVersionChanged");
    } else {
        msg = brls::getStr("page/modList/mhriseVersionUnsupported");
    }

    CustomDialog::show(msg, {
        {brls::getStr("page/modList/ok"), [] { CustomDialog::close(); }},
    });
    return false;
}

bool ModList::checkDontStarveRules(int index) {
    std::string tid = format::appIdHex(m_modManager.game().appId);
    auto result = ModInstaller::dontStarve::checkBeforeInstall(tid);
    if (result == ModInstaller::dontStarve::InstallPrecheckResult::Ok) return true;

    auto onContinue = [this, index] { CustomDialog::close([this, index] { startModInstallTask(index); }); };
    CustomDialog::show(brls::getStr("page/modList/dontStarveFrameworkMissing"), {
        {brls::getStr("page/modList/cancel"), [] { CustomDialog::close(); }},
        {brls::getStr("page/modList/confirm"), onContinue},
    });
    return false;
}

void ModList::toggleModInstall(int index) {
    auto& mod = m_modManager.mods()[index];
    bool installing = !mod.isInstalled;
    if (installing && !checkBeforeModInstall(index)) return;

    showModInstallDialog(index);
}

void ModList::startModInstallTask(int index) {
    auto& mod = m_modManager.mods()[index];
    bool installing = !mod.isInstalled;

    std::string modName = mod.displayName;

    deviceControl::HomeButton::disable();
    deviceControl::CpuBoost::enableFastLoad();
    m_installStop = std::stop_source{};
    auto installToken = m_installStop.get_token();
    auto pageToken = m_stopSource.get_token();

    std::string title = installing ? brls::getStr("page/modList/installing", modName) : brls::getStr("page/modList/uninstalling", modName);
    if (installing) {
        auto onCancel = [this] { m_installStop.request_stop(); };
        ProgressDialog::show(title, {{brls::getStr("page/modList/cancel"), onCancel}}, onCancel);
    } else {
        ProgressDialog::show(title, {}, [] {});
    }

    auto cleaningStarted = std::make_shared<bool>(false);

    auto progressCb = [pageToken, cleaningStarted, modName](const ModInstaller::Progress& progress) {
        bool cleaning    = progress.cleaning;
        int current      = progress.current;
        int total        = progress.total;
        std::string file = progress.currentFile;
        int64_t written  = progress.bytesWritten;
        int64_t fileSize = progress.bytesTotal;

        brls::sync([=] {
            if (pageToken.stop_requested()) return;

            if (cleaning && !*cleaningStarted) {
                *cleaningStarted = true;
                Audio::instance()->play(SoundEffect::Warning);
                ProgressDialog::setTitle(brls::getStr("page/modList/cleaning", modName));
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

    m_installTask = ThreadPool::instance().submitWaitable([this, index, installing, progressCb, pageToken](std::stop_token token) {
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
        brls::sync([this, index, installing, success, cancelled, errorMsg = std::move(errorMsg), errorFile = std::move(errorFile), conflictMod = std::move(conflictMod), elapsed, pageToken] {
            deviceControl::CpuBoost::disable();
            deviceControl::HomeButton::enable();
            if (pageToken.stop_requested()) return;

            if (success) {
                m_gameManager.setHasInstalledMod(m_gameIndex, installing || ModManager::hasInstalledMod(m_modManager.game().dirPath));
                std::string msg = installing ? brls::getStr("page/modList/installSuccess", elapsed) : brls::getStr("page/modList/uninstallSuccess", elapsed);
                auto onClose = [this, index] { CustomDialog::close([this, index] { refreshAndFocus(index); }); };
                CustomDialog::show(msg, {{brls::getStr("page/modList/ok"), onClose}}, onClose);
                return;
            }

            std::string msg;
            if (cancelled) msg = brls::getStr("page/modList/installCancelled");
            else if (!conflictMod.empty()) msg = brls::getStr("page/modList/installConflict", conflictMod, errorFile);
            else msg = (installing ? brls::getStr("page/modList/installFailed", errorMsg, errorFile) : brls::getStr("page/modList/uninstallFailed", errorMsg, errorFile));
            CustomDialog::show(msg, {{brls::getStr("page/modList/ok"), [] { CustomDialog::close(); }}});
        });
    }, installToken);
}

void ModList::showModInstallDialog(int index) {
    bool installing = !m_modManager.mods()[index].isInstalled;
    auto onConfirm = [this, index] { startModInstallTask(index); };

    std::string confirmMsg = installing ? brls::getStr("page/modList/confirmInstall") : brls::getStr("page/modList/confirmUninstall");

    CustomDialog::show(confirmMsg, {
        {brls::getStr("page/modList/cancel"), [] { CustomDialog::close(); }},
        {brls::getStr("page/modList/confirm"), onConfirm},
    });
}

void ModList::toggleModDisable() {
    bool disabling = !m_modManager.game().isModsDisabled;

    auto onConfirm = [this, disabling] {
        std::string msg = disabling ? brls::getStr("page/modList/disabling") : brls::getStr("page/modList/enabling");
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
                    CustomDialog::show(brls::getStr("page/modList/disableFailed"), {{brls::getStr("page/modList/ok"), [] { CustomDialog::close(); }}});
                    return;
                }
                if (!disabling) m_gameManager.setModsDisabled(m_gameIndex, false);
                std::string doneMsg = disabling ? brls::getStr("page/modList/disableSuccess") : brls::getStr("page/modList/enableSuccess");
                auto onClose = [this] { CustomDialog::close([this] { refreshAndFocus(m_focusedIndex); }); };
                CustomDialog::show(doneMsg, {{brls::getStr("page/modList/ok"), onClose}}, onClose);
            });
        }, installToken);
    };

    std::string confirmMsg = disabling ? brls::getStr("page/modList/confirmDisable") : brls::getStr("page/modList/confirmEnable");
    CustomDialog::show(confirmMsg, {
        {brls::getStr("page/modList/cancel"), [] { CustomDialog::close(); }},
        {brls::getStr("page/modList/confirm"), onConfirm},
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
        ShellState::setIndexText(std::to_string(index + 1) + " / " + std::to_string(m_modManager.mods().size()));
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
    updateActionHint(brls::BUTTON_Y, m_modManager.sortAsc() ? brls::getStr("page/modList/sortAsc") : brls::getStr("page/modList/sortDesc"));
    brls::Application::getGlobalHintsUpdateEvent()->fire();
}

void ModList::refreshAndFocus(int index) {
    m_grid->setDefaultCellFocus(index);
    m_grid->reloadData();
    m_grid->instantFocus(index);
    m_focusedIndex = index;
    updateDetail(index);
}

int ModList::finalizeModAddition() {
    std::string lastDir = m_modManager.mods().back().dirName;
    m_modManager.sort();
    int newIdx = std::max(0, m_modManager.findByDirName(lastDir));

    auto& game = m_gameManager.games()[m_gameIndex];
    game.modCount = std::to_string(m_modManager.mods().size());
    m_gameManager.setPendingFocus(game.appId);
    return newIdx;
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
    m_tagAuthor->setText(mod.author.empty() ? brls::getStr("page/modList/unknownAuthor") : brls::getStr("page/modList/authorPrefix", mod.author));
    m_tagFormat->setText(mod.isZip ? brls::getStr("page/modList/zipType") : brls::getStr("page/modList/fileType"));
    m_tagSize->setText(mod.size.empty() ? brls::getStr("page/modList/calculatingSize") : mod.size);
    m_tagVersion->setText(mod.modVersion.empty() ? brls::getStr("page/modList/modVersionUnknown") : brls::getStr("page/modList/modVersionFmt", format::cleanVersion(mod.modVersion)));
    if (mod.gameVersion.empty()) m_tagGameVer->setText(brls::getStr("page/modList/gameVersionUnknown"));
    else if (mod.gameVersion == "0") m_tagGameVer->setText(brls::getStr("page/modList/gameVersionUniversal"));
    else m_tagGameVer->setText(brls::getStr("page/modList/gameVersionFmt", format::cleanVersion(mod.gameVersion)));
    m_descBody->setText(mod.description.empty() ? brls::getStr("page/modList/noDescription") : mod.description);
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

void ModList::submitNextCrc() {
    m_crcLoading = true;
    auto token = m_stopSource.get_token();
    auto& mods = m_modManager.mods();

    // 主线程找离焦点最近的需计算 CRC32 的 mod（仅商店 mod 且 crc32 为空）
    int center = m_focusedIndex;
    int bestIdx = -1;
    int bestDist = INT_MAX;
    for (int i = 0; i < static_cast<int>(mods.size()); i++) {
        if (mods[i].modID > 0 && mods[i].fileCrc32.empty()) {
            int dist = std::abs(i - center);
            if (dist < bestDist) { bestDist = dist; bestIdx = i; }
        }
    }

    if (bestIdx < 0) {
        m_crcLoading = false;
        m_modManager.saveJson();
        submitUpdateCheck();
        return;
    }

    int modIdx = bestIdx;
    std::string zipPath = mods[modIdx].path + "/" + mods[modIdx].dirName + ".zip";

    ThreadPool::instance().submit([this, modIdx, zipPath = std::move(zipPath)](std::stop_token token) {
        std::vector<char> buf(64 * 1024);
        int64_t crc = crc::fromFile(zipPath.c_str(), buf.data(), buf.size(), &token);
        if (token.stop_requested()) return;
        std::string crc32;
        if (crc >= 0) {
            char hex[9];
            std::snprintf(hex, sizeof(hex), "%08x", static_cast<uint32_t>(crc));
            crc32 = hex;
        }

        brls::sync([this, modIdx, crc32 = std::move(crc32), token]() {
            if (token.stop_requested()) return;
            if (!crc32.empty()) m_modManager.setFileCrc32(modIdx, crc32);
            submitNextCrc();
        });
    }, token);
}

void ModList::submitUpdateCheck() {
    if (!deviceInfo::Network::isAvailable()) return;

    std::string modsJson = m_modManager.buildUpdateCheckJson();
    if (modsJson.empty()) return;

    std::string gameTid = format::appIdHex(m_modManager.game().appId);

    ThreadPool::instance().submit([this, gameTid = std::move(gameTid), modsJson = std::move(modsJson)](std::stop_token token) {
        auto ids = m_modManager.checkForUpdates(gameTid, modsJson, token);
        if (ids.empty() || token.stop_requested()) return;
        brls::sync([this, ids = std::move(ids), token] {
            if (token.stop_requested()) return;
            auto& mods = m_modManager.mods();
            for (int id : ids) {
                int idx = m_modManager.findByModID(id);
                if (idx < 0) continue;
                mods[idx].hasUpdate = true;
                auto* cell = m_grid->getGridItemByIndex(idx);
                if (cell) static_cast<ModCard*>(cell)->setMod(mods[idx].displayName, mods[idx].type, mods[idx].isInstalled, m_modManager.game().isModsDisabled, mods[idx].modID, mods[idx].hasUpdate);
            }
        });
    }, m_stopSource.get_token());
}

void ModList::applyModDisplayName(int idx, const std::string& name) {
    m_modManager.setDisplayName(idx, name);
    auto& mod = m_modManager.mods()[idx];
    auto* cell = m_grid->getGridItemByIndex(idx);
    if (cell) static_cast<ModCard*>(cell)->setMod(mod.displayName, mod.type, mod.isInstalled, m_modManager.game().isModsDisabled, mod.modID, mod.hasUpdate);
}

void ModList::manualSetModDisplayName() {
    int idx = m_focusedIndex;
    std::string name = keyboard::showText(brls::getStr("page/modList/inputModName"), brls::getStr("page/modList/inputModName"), m_modManager.mods()[idx].displayName, 64);
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
        if (cell) static_cast<ModCard*>(cell)->setMod(mod.displayName, mod.type, mod.isInstalled, m_modManager.game().isModsDisabled, mod.modID, mod.hasUpdate);
    };
    CustomDialog::show(brls::getStr("page/modList/restoreNameMsg", restored), {
        {brls::getStr("page/modList/cancel"), [] { CustomDialog::close(); }},
        {brls::getStr("page/modList/restoreNameOk"), [onConfirm] { CustomDialog::close(onConfirm); }},
    });
}

void ModList::applyModType(int idx, const std::string& type) {
    m_modManager.setType(idx, type);
    auto& mod = m_modManager.mods()[idx];
    auto* cell = m_grid->getGridItemByIndex(idx);
    if (cell) static_cast<ModCard*>(cell)->setMod(mod.displayName, mod.type, mod.isInstalled, m_modManager.game().isModsDisabled, mod.modID, mod.hasUpdate);
    if (static_cast<size_t>(idx) == m_lastFocusIndex) m_tagType->setText(modTypeText(type));
}

void ModList::editModDescription() {
    int idx = m_focusedIndex;
    std::string desc = keyboard::showText(brls::getStr("page/modList/inputModDesc"), brls::getStr("page/modList/inputModDesc"), m_modManager.mods()[idx].description, 500);
    if (desc.empty()) return;
    m_modManager.setDescription(idx, desc);
    if (static_cast<size_t>(idx) == m_lastFocusIndex) m_descBody->setText(desc);
}

void ModList::editModVersion() {
    int idx = m_focusedIndex;
    std::string ver = keyboard::showNumber(brls::getStr("page/modList/inputModVersion"), m_modManager.mods()[idx].modVersion, 20);
    if (ver.empty()) return;
    m_modManager.setModVersion(idx, ver);
    updateDetail(idx);
}

void ModList::editGameVersion() {
    int idx = m_focusedIndex;
    std::string ver = keyboard::showNumber(brls::getStr("page/modList/inputGameVersion"), m_modManager.mods()[idx].gameVersion, 20);
    if (ver.empty()) return;
    m_modManager.setGameVersion(idx, ver);
    updateDetail(idx);
}

void ModList::editModAuthor() {
    int idx = m_focusedIndex;
    std::string author = keyboard::showText(brls::getStr("page/modList/inputModAuthor"), brls::getStr("page/modList/inputModAuthor"), m_modManager.mods()[idx].author, 34);
    if (author.empty()) return;
    m_modManager.setAuthor(idx, author);
    updateDetail(idx);
}

void ModList::setupEditMenu() {
    m_editMenu.title = brls::getStr("page/modList/editMenuTitle");

    auto& editNameItem = m_editMenu.addItem(brls::getStr("page/modList/editName"), brls::getStr("page/modList/editNameDesc"));
    editNameItem.setDisabled([this]{ return m_modManager.mods().empty(); });
    editNameItem.setStayOpen();
    editNameItem.setAction([this]{ manualSetModDisplayName(); });

    auto& restoreNameItem = m_editMenu.addItem(brls::getStr("page/modList/editRestoreName"), brls::getStr("page/modList/editRestoreNameDesc"));
    restoreNameItem.setDisabled([this]{ return m_modManager.mods().empty(); });
    restoreNameItem.setBadge([this]{
        if (m_modManager.mods().empty()) return std::string();
        return m_modManager.mods()[m_focusedIndex].dirName;
    });
    restoreNameItem.setStayOpen();
    restoreNameItem.setAction([this]{ resetModDisplayName(); });

    auto& editTypeItem = m_editMenu.addItem(brls::getStr("page/modList/editType"), brls::getStr("page/modList/editTypeDesc"));
    editTypeItem.setDisabled([this]{ return m_modManager.mods().empty(); });
    editTypeItem.setBadge([this]{
        if (m_modManager.mods().empty()) return std::string();
        return modTypeText(m_modManager.mods()[m_focusedIndex].type);
    });
    editTypeItem.setSubmenu(&m_typeMenu);

    auto& editDescItem = m_editMenu.addItem(brls::getStr("page/modList/editDesc"), brls::getStr("page/modList/editDescDesc"));
    editDescItem.setDisabled([this]{ return m_modManager.mods().empty(); });
    editDescItem.setStayOpen();
    editDescItem.setAction([this]{ editModDescription(); });

    auto& editModVerItem = m_editMenu.addItem(brls::getStr("page/modList/editModVer"), brls::getStr("page/modList/editModVerDesc"));
    editModVerItem.setDisabled([this]{ return m_modManager.mods().empty(); });
    editModVerItem.setBadge([this]{
        if (m_modManager.mods().empty()) return std::string();
        auto& v = m_modManager.mods()[m_focusedIndex].modVersion;
        return v.empty() ? brls::getStr("page/modList/unknown") : v;
    });
    editModVerItem.setStayOpen();
    editModVerItem.setAction([this]{ editModVersion(); });

    auto& editGameVerItem = m_editMenu.addItem(brls::getStr("page/modList/editGameVer"), brls::getStr("page/modList/editGameVerDesc"));
    editGameVerItem.setDisabled([this]{ return m_modManager.mods().empty(); });
    editGameVerItem.setBadge([this]{
        if (m_modManager.mods().empty()) return std::string();
        auto& v = m_modManager.mods()[m_focusedIndex].gameVersion;
        if (v.empty()) return brls::getStr("page/modList/unknown");
        if (v == "0") return brls::getStr("page/modList/versionUniversal");
        return v;
    });
    editGameVerItem.setStayOpen();
    editGameVerItem.setAction([this]{ editGameVersion(); });

    auto& editAuthorItem = m_editMenu.addItem(brls::getStr("page/modList/editAuthor"), brls::getStr("page/modList/editAuthorDesc"));
    editAuthorItem.setDisabled([this]{ return m_modManager.mods().empty(); });
    editAuthorItem.setBadge([this]{
        if (m_modManager.mods().empty()) return std::string();
        auto& v = m_modManager.mods()[m_focusedIndex].author;
        return v.empty() ? brls::getStr("page/modList/unknown") : v;
    });
    editAuthorItem.setStayOpen();
    editAuthorItem.setAction([this]{ editModAuthor(); });

    // ── 类型选择子菜单 ──
    m_typeMenu.title = brls::getStr("page/modList/typeMenuTitle");

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
        deviceControl::HomeButton::disable();
        ProgressDialog::show(brls::getStr("page/modList/forceCleanProgress"), {}, nullptr);

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
                deviceControl::HomeButton::enable();
                if (pageToken.stop_requested()) return;

                std::string msg;
                switch (result.status) {
                    case fs::RemoveResult::Completed:
                        msg = brls::getStr("page/modList/forceCleanCompleted", result.elapsed);
                        m_gameManager.setModsDisabled(m_gameIndex, false);
                        m_gameManager.setHasInstalledMod(m_gameIndex, false);
                        break;
                    case fs::RemoveResult::FsError:
                        msg = brls::getStr("page/modList/forceCleanFailed", result.errorPath, result.errorCode);
                        break;
                    default: break;
                }
                auto onClose = [this] { CustomDialog::close([this] { m_modManager.sort(); refreshAndFocus(0); }); };
                CustomDialog::show(msg, {{brls::getStr("page/modList/ok"), onClose}}, onClose);
            });
        }, installToken);
    };

    CustomDialog::show(brls::getStr("page/modList/forceCleanConfirm"), {
        {brls::getStr("page/modList/cancel"), [] { CustomDialog::close(); }},
        {brls::getStr("page/modList/cancel"), [] { CustomDialog::close(); }},
        {brls::getStr("page/modList/forceCleanBtn"), onConfirm},
    });
}

void ModList::setupManageMenu() {
    m_manageMenu.title = brls::getStr("page/modList/manageMenuTitle");

    auto& editSubmenuItem = m_manageMenu.addItem(brls::getStr("page/modList/editSubmenu"), brls::getStr("page/modList/editSubmenuDesc"));
    editSubmenuItem.setDisabled([this]{ return m_modManager.mods().empty(); });
    editSubmenuItem.setBadge("\uE14A");
    editSubmenuItem.setSubmenu(&m_editMenu);

    auto& removeModItem = m_manageMenu.addItem(brls::getStr("page/modList/removeMod"), brls::getStr("page/modList/removeModDesc"));
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

    auto& viewPathItem = m_manageMenu.addItem(brls::getStr("page/modList/viewPath"), brls::getStr("page/modList/viewPathDesc"));
    viewPathItem.setDisabled([this]{ return m_modManager.mods().empty(); });
    viewPathItem.setBadge([this]() {
        if (m_modManager.mods().empty()) return std::string();
        return m_modManager.mods()[m_focusedIndex].dirName;
    });
    viewPathItem.setAction([this]{
        auto& mod = m_modManager.mods()[m_focusedIndex];
        CustomDialog::show(mod.path, {{brls::getStr("page/modList/ok"), [] { CustomDialog::close(); }}});
    });

}

void ModList::addModsFromTransit() {
    auto mods = ModManager::scanTransitMods();
    if (mods.empty()) {
        CustomDialog::show(brls::getStr("page/modList/transitEmpty"), {{brls::getStr("page/modList/ok"), [] { CustomDialog::close(); }}});
        return;
    }

    auto* menu = new MenuPageConfig();
    menu->title = brls::getStr("page/modList/selectMod");
    menu->multiSelect = true;
    for (const auto& mod : mods){
        menu->addItem(mod.name);
    }
        
    menu->onConfirm = [this, mods = std::move(mods)](const std::vector<int>& selected) mutable {
        CustomDialog::show(brls::getStr("page/modList/addModConfirm", std::to_string(selected.size())), {
            {brls::getStr("page/modList/cancel"), [] { CustomDialog::close(); }},
            {brls::getStr("page/modList/confirm"), [this, mods = std::move(mods), selected] {
                std::vector<fs::DirEntry> chosen;
                for (int i : selected) chosen.push_back(mods[i]);

                int added = m_modManager.addModsFormTransitForModList(chosen);
                int newIdx = finalizeModAddition();

                auto onClose = [this, newIdx] { CustomDialog::close([this, newIdx] { refreshAndFocus(newIdx); submitNextSize(); }); };
                CustomDialog::show(brls::getStr("page/modList/addModSuccess", std::to_string(added)), {{brls::getStr("page/modList/ok"), onClose}}, onClose);
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

    CustomDialog::show(brls::getStr("page/modList/removeModConfirm"), {
        {brls::getStr("page/modList/cancel"), [] { CustomDialog::close(); }},
        {brls::getStr("page/modList/cancel"), [] { CustomDialog::close(); }},
        {brls::getStr("page/modList/removeModBtn"), onConfirm},
    });
}

void ModList::removeLastModFromList() {
    auto onConfirm = [this] {
        auto& game = m_gameManager.games()[m_gameIndex];
        m_gameManager.setPendingRemove(game.appId);
        CustomDialog::close([this] {
            Page::popPage();
        });
    };

    CustomDialog::show(brls::getStr("page/modList/removeLastModConfirm"), {
        {brls::getStr("page/modList/cancel"), [] { CustomDialog::close(); }},
        {brls::getStr("page/modList/cancel"), [] { CustomDialog::close(); }},
        {brls::getStr("page/modList/removeModBtn"), onConfirm},
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
    m_menu.title = brls::getStr("page/modList/mainMenuTitle");

    m_menu.shouldShowFakeHighlight = [this]{ return !m_modManager.mods().empty(); };

    auto& addProjectItem = m_menu.addItem(brls::getStr("page/modList/addProjectSubmenu"), brls::getStr("page/modList/addProjectSubmenuDesc"));
    addProjectItem.setBadge("\uE14A");
    addProjectItem.setSubmenu(&m_addProjectMenu);

    auto& viewModItem = m_menu.addItem(brls::getStr("page/modList/viewModSubmenu"), brls::getStr("page/modList/viewModSubmenuDesc"));
    viewModItem.setDisabled([this] {
        if (m_modManager.mods().empty()) return true;
        auto idx = m_focusedIndex;
        if (idx >= static_cast<int>(m_modManager.mods().size())) return true;
        return m_modManager.mods()[idx].modID < 0;
    });
    viewModItem.setBadge("\uE14A");
    viewModItem.setSubmenu(&m_viewModMenu);

    auto& manageSubmenuItem = m_menu.addItem(brls::getStr("page/modList/manageSubmenu"), brls::getStr("page/modList/manageSubmenuDesc"));
    manageSubmenuItem.setBadge("\uE14A");
    manageSubmenuItem.setSubmenu(&m_manageMenu);

    auto& disableItem = m_menu.addItem(brls::getStr("page/modList/disableMod"), brls::getStr("page/modList/disableModDesc"));
    disableItem.setDisabled([this]{ return m_modManager.mods().empty(); });
    disableItem.setBadge([this]{ return m_modManager.game().isModsDisabled ? brls::getStr("page/modList/on") : brls::getStr("page/modList/off"); });
    disableItem.setBadgeHighlight([this]{ return m_modManager.game().isModsDisabled; });
    disableItem.setAction([this]{ toggleModDisable(); });

    auto& forceCleanItem = m_menu.addItem(brls::getStr("page/modList/forceClean"), brls::getStr("page/modList/forceCleanDesc"));
    forceCleanItem.setDisabled([this]{ return m_modManager.mods().empty(); });
    forceCleanItem.setAction([this]{ forceClean(); });
}

void ModList::setupViewModMenu() {
    m_viewModMenu.title = brls::getStr("page/modList/viewModSubmenu");

    auto& modDetailItem = m_viewModMenu.addItem(brls::getStr("page/modList/storeModDetail"), brls::getStr("page/modList/storeModDetailDesc"));
    modDetailItem.setBadge("\uE14A");
    modDetailItem.setAction([this] {
        auto& mod = m_modManager.mods()[m_focusedIndex];
        auto& game = m_gameManager.games()[m_gameIndex];
        std::string tid = format::appIdHex(game.appId);
        Page::pushPage(new StoreModDetail(mod.modID, tid, game.displayName, m_gameManager, nullptr, &m_modManager, game.version, true), PageAnimType::SlideFromLeft);
    });

    auto& viewAuthorItem = m_viewModMenu.addItem(brls::getStr("page/modList/viewAuthor"), brls::getStr("page/modList/viewAuthorDesc"));
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
    m_localAddMenu.title = brls::getStr("page/modList/localAdd");

    auto& selectModItem = m_localAddMenu.addItem(brls::getStr("page/modList/selectMod"), brls::getStr("page/modList/selectModDesc"));
    selectModItem.setBadge([]{ int n = ModManager::transitModCount(); return std::to_string(n) + " MOD"; });
    selectModItem.setAction([this]{ addModsFromTransit(); });

    auto& mtpItem = m_localAddMenu.addItem(brls::getStr("page/modList/mtpTransfer"), brls::getStr("page/modList/mtpTransferDesc"));
    mtpItem.setAction([] {
        std::vector<MtpMount> mounts = {
            {"/mods2/!temp_mods", "Add Mod"},
        };
        mtpDialog::open(mounts);
    });

    auto& ftpItem = m_localAddMenu.addItem(brls::getStr("page/modList/ftpTransfer"), brls::getStr("page/modList/ftpTransferDesc"));
    ftpItem.setAction([] {
        std::vector<FtpMount> mounts = {
            {"/mods2/!temp_mods", "Add Mod"},
        };
        ftpDialog::open(mounts);
    });
}

void ModList::setupAddProjectMenu() {
    m_addProjectMenu.title = brls::getStr("page/modList/addProjectSubmenu");

    auto& storeAddItem = m_addProjectMenu.addItem(brls::getStr("page/modList/storeAddSubmenu"), brls::getStr("page/modList/storeAddSubmenuDesc"));
    storeAddItem.setBadge("\uE14A");
    storeAddItem.setSubmenu(&m_storeAddMenu);

    auto& localAddItem = m_addProjectMenu.addItem(brls::getStr("page/modList/localAdd"), brls::getStr("page/modList/localAddDesc"));
    localAddItem.setBadge("\uE14A");
    localAddItem.setSubmenu(&m_localAddMenu);
}

void ModList::setupStoreAddMenu() {
    m_storeAddMenu.title = brls::getStr("page/modList/storeAddSubmenu");

    auto& modListItem = m_storeAddMenu.addItem(brls::getStr("page/modList/storeModList"), brls::getStr("page/modList/storeModListDesc"));
    modListItem.setBadge("\uE14A");
    modListItem.setAction([this] {
        auto& game = m_gameManager.games()[m_gameIndex];
        std::string tid = format::appIdHex(game.appId);
        Page::pushPage(new StoreModList(tid, game.displayName, m_gameManager, &m_modManager, game.version, true), PageAnimType::SlideFromLeft);
    });

    auto& uploadItem = m_storeAddMenu.addItem(brls::getStr("page/modList/uploadMod"), brls::getStr("page/modList/uploadModDesc"));
    uploadItem.setBadge("\uE14A");
    uploadItem.setAction([] {
        QrCodeView::show(config::websiteUrl);
    });
    uploadItem.setStayOpen();
}

brls::Box* ModList::createFirstLaunchBox() {
    LongTextBoxConfig content;

    auto& basic = content.addEntry();
    basic.addTitle(brls::getStr("page/modList/firstLaunchBasicTitle"));
    basic.addBody(brls::getStr("page/modList/firstLaunchBasicBody"));

    auto& shortcuts = content.addEntry();
    shortcuts.addTitle(brls::getStr("page/modList/firstLaunchShortcutsTitle"));
    shortcuts.addBody(brls::getStr("page/modList/firstLaunchShortcutsBody"), 1.3f);

    auto& localAdd = content.addEntry();
    localAdd.addTitle(brls::getStr("page/modList/firstLaunchLocalAddTitle"));
    localAdd.addBody(brls::getStr("page/modList/firstLaunchLocalAddBody"));

    auto& notice = content.addEntry();
    notice.addTitle(brls::getStr("page/modList/firstLaunchNoticeTitle"));
    notice.addBody(brls::getStr("page/modList/firstLaunchNoticeBody"));

    return LongTextBox::create(content);
}

brls::Box* ModList::createMHRiseFirstLaunchBox() {
    LongTextBoxConfig content;

    auto& notice = content.addEntry();
    notice.addTitle(brls::getStr("page/modList/mhriseFirstLaunchTitle"));
    notice.addBody(brls::getStr("page/modList/mhriseFirstLaunchBody"));

    auto& troubleshooting = content.addEntry();
    troubleshooting.addTitle(brls::getStr("page/modList/mhriseFirstLaunchTroubleshootingTitle"));
    troubleshooting.addBody(brls::getStr("page/modList/mhriseFirstLaunchTroubleshootingBody"));

    return LongTextBox::create(content);
}

brls::Box* ModList::createDontStarveFirstLaunchBox() {
    LongTextBoxConfig content;

    auto& notice = content.addEntry();
    notice.addTitle(brls::getStr("page/modList/dontStarveFirstLaunchTitle"));
    notice.addBody(brls::getStr("page/modList/dontStarveFirstLaunchBody"));

    auto& usage = content.addEntry();
    usage.addTitle(brls::getStr("page/modList/dontStarveFirstLaunchUsageTitle"));
    usage.addBody(brls::getStr("page/modList/dontStarveFirstLaunchUsageBody"));

    auto& troubleshooting = content.addEntry();
    troubleshooting.addTitle(brls::getStr("page/modList/dontStarveFirstLaunchTroubleshootingTitle"));
    troubleshooting.addBody(brls::getStr("page/modList/dontStarveFirstLaunchTroubleshootingBody"));

    return LongTextBox::create(content);
}

brls::Box* ModList::createUnmanagedModBox() {
    LongTextBoxConfig content;

    auto& detected = content.addEntry();
    detected.addTitle(brls::getStr("page/modList/unmanagedDetectedTitle"));
    detected.addBody(brls::getStr("page/modList/unmanagedDetectedBody"));

    auto& extraction = content.addEntry();
    extraction.addTitle(brls::getStr("page/modList/unmanagedExtractionTitle"));
    extraction.addBody(brls::getStr("page/modList/unmanagedExtractionBody"));

    auto& notice = content.addEntry();
    notice.addTitle(brls::getStr("page/modList/unmanagedNoticeTitle"));
    notice.addBody(brls::getStr("page/modList/unmanagedNoticeBody"));

    return LongTextBox::create(content);
}

void ModList::showUnmanagedModDialog() {
    if (!m_modManager.hasUnmanagedMods()) return;

    auto onBack = [this] {
        ScrollDialog::close([this] { Page::popPage(); });
    };

    ScrollDialog::show(createUnmanagedModBox(), brls::getStr("page/modList/back"), onBack, brls::getStr("page/modList/confirm"), [this] { startUnmanagedModExtraction(); }, onBack);
}

void ModList::startUnmanagedModExtraction() {
    deviceControl::HomeButton::disable();
    auto result = m_modManager.extractUnmanagedMods();
    deviceControl::HomeButton::enable();

    if (!result.success) {
        std::string message = result.rollbackFailed ? brls::getStr("page/modList/unmanagedRollbackFailed", result.errorPath) : brls::getStr("page/modList/unmanagedExtractFailed", result.errorPath);
        auto returnHome = [this] { CustomDialog::close([this] { Page::popPage(); }); };
        CustomDialog::show(message, {{brls::getStr("page/modList/backToHome"), returnHome}}, [] {});
        return;
    }

    int newIdx = finalizeModAddition();

    ScrollDialog::close([this, newIdx] {
        refreshAndFocus(newIdx);
        submitNextSize();
        submitNextCrc();
    });
}

void ModList::showMHRiseFirstLaunchDialog() {
    if (!Settings::getBool("modList", "mhriseFirstLaunch", true)) {
        showUnmanagedModDialog();
        return;
    }

    auto onConfirm = [this]() {
        Settings::setBool("modList", "mhriseFirstLaunch", false);
        LongPressDialog::close([this]() { showUnmanagedModDialog(); });
    };
    LongPressDialog::show(createMHRiseFirstLaunchBox(), brls::getStr("page/modList/firstLaunchBtn"), 5.0f, onConfirm);
}

void ModList::showDontStarveFirstLaunchDialog() {
    if (!Settings::getBool("modList", "dontStarveFirstLaunch", true)) {
        showUnmanagedModDialog();
        return;
    }

    auto onConfirm = [this]() {
        Settings::setBool("modList", "dontStarveFirstLaunch", false);
        LongPressDialog::close([this]() { showUnmanagedModDialog(); });
    };
    LongPressDialog::show(createDontStarveFirstLaunchBox(), brls::getStr("page/modList/firstLaunchBtn"), 5.0f, onConfirm);
}

void ModList::showSpecialGameFirstLaunchDialog() {
    switch (m_modManager.modGameType()) {
        case ModGameType::MHRise:
            showMHRiseFirstLaunchDialog();
            return;

        case ModGameType::DontStarve:
            showDontStarveFirstLaunchDialog();
            return;

        case ModGameType::Normal:
        default:
            showUnmanagedModDialog();
            return;
    }
}

void ModList::runFirstLaunchDialog() {
    bool isFirstLaunch = Settings::getBool("modList", "firstLaunch", true);
    if (!isFirstLaunch) {
        brls::sync([this]() { showSpecialGameFirstLaunchDialog(); });
        return;
    }
    brls::sync([this]() {
        auto onConfirm = [this]() {
            brls::sync([this]() {
                Settings::setBool("modList", "firstLaunch", false);
                LongPressDialog::close([this]() {
                    showSpecialGameFirstLaunchDialog();
                });
            });
        };
        LongPressDialog::show(createFirstLaunchBox(), brls::getStr("page/modList/firstLaunchBtn"), 5.0f, onConfirm);
    });
}
