/**
 * ModList - Mod 列表页面
 * 左侧 RecyclingGrid（单列 Mod 列表） + 右侧 Mod 详情
 */

#include "ui/page/modList.hpp"
#include <borealis/core/i18n.hpp>
#include "api/mod.hpp"
#include "core/audio.hpp"
#include "core/frameQueue.hpp"
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
#include "ui/navigation/navigationGroups.hpp"
#include "ui/view/qrCodeView.hpp"
#include "ui/page/storeModList.hpp"
#include "ui/page/storeModDetail.hpp"
#include "common/settings.hpp"
#include "ui/view/dialog/longPressDialog.hpp"
#include "ui/view/longTextBox.hpp"
#include "ui/view/dialog/scrollDialog.hpp"
#include "utils/keyboard.hpp"
// #include "utils/pageNav.hpp"
#include "utils/crc32.hpp"
#include <borealis/core/cache_helper.hpp>
#include <chrono>
#include <climits>
#include <yoga/Yoga.h>
#include <switch.h>

ModList::ModList(size_t gameIndex, GameManager& gameManager)
    : m_gameManager(gameManager), m_gameIndex(gameIndex), m_modManager(gameManager.games()[gameIndex]) {
    inflateFromXMLRes("xml/view/page/modList.xml");

    setHeader();
}

ModList::~ModList() {
    m_stopSource.request_stop();
    m_installStop.request_stop();
    if (m_iconRetryDelayId != 0) brls::cancelDelay(m_iconRetryDelayId);
    m_installTask.wait();
}

void ModList::onResume() {
    int modID = m_modManager.consumePendingFocus();
    if (modID < 0) return;

    int newIdx = m_modManager.findByModID(modID);
    if (newIdx < 0) return;

    m_grid->deferReload(newIdx);
    m_focusedIndex = newIdx;
}

void ModList::setHeader() {
    std::string contentTitle = brls::getStr("page/modList/notInstalled");
    if (m_modManager.game().version != "...") contentTitle = format::cleanVersion(m_modManager.game().version);

    HeaderState headerState;
    headerState.setNavigation(createModNavigationState(ModNavigationPage::ModList));
    headerState.setContentTitle(contentTitle);
    ShellState::setHeaderState(headerState);
}

void ModList::onContentAvailable() {
    setupDetail();
    setupModGrid();

    if (!m_modManager.mods().empty()) updateDetail(0);

    // ZL：进入商店页面
    registerAction("", brls::BUTTON_LT, [this](...) {
        Audio::instance()->play(SoundEffect::Enter);
        auto& game = m_gameManager.games()[m_gameIndex];
        std::string tid = format::appIdHex(game.appId);
        Page::pushPage(new StoreModList(tid, game.displayName, tid, m_gameManager, &m_modManager, game.version, true), PageAnimType::SlideFromLeft);
        return true;
    }, true);

    // ZR：进入当前模组商店详情
    registerAction("", brls::BUTTON_RT, [this](...) {
        Audio::instance()->play(SoundEffect::Enter);
        openStoreModDetail();
        return true;
    }, true);

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

    if (!m_modManager.hasUnmanagedMods()) submitNextCard();
    runFirstLaunchDialog();

    m_layoutReady = true;
}

void ModList::onLayout() {
    Page::onLayout();
    if (m_layoutReady) updateScrollHintVisibility();
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
                m_modManager.setInstalled(index, installing);
                m_gameManager.setHasInstalledMod(m_gameIndex, installing || ModManager::hasInstalledMod(m_modManager.game().dirPath));
                std::string msg = installing ? brls::getStr("page/modList/installSuccess", elapsed) : brls::getStr("page/modList/uninstallSuccess", elapsed);
                auto onClose = [this, index] {
                    CustomDialog::close([this, index] {
                        refreshAndFocus(index);
                    });
                };
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
    auto pageToken = m_stopSource.get_token();

    // 禁用时先写标志再做文件操作：异常中断后标志已是禁用状态，下次启动仍可走恢复流程
    if (disabling) m_gameManager.setModsDisabled(m_gameIndex, true);

    bool ok;
    if (disabling) ok = m_modManager.disableMods();
    else ok = m_modManager.enableMods();
    if (ok && !disabling) m_gameManager.setModsDisabled(m_gameIndex, false);

    brls::sync([this, ok, pageToken] {
        if (pageToken.stop_requested()) return;
        if (!ok) {
            CustomDialog::show(brls::getStr("page/modList/disableFailed"), {{brls::getStr("page/modList/ok"), [] { CustomDialog::close(); }}});
            return;
        }

        auto& mods = m_modManager.mods();
        bool disabled = m_modManager.game().isModsDisabled;
        for (auto* item : m_grid->getGridItems()) {
            auto* card = dynamic_cast<ModCard*>(item);
            if (!card) continue;
            auto& mod = mods[card->getIndex()];
            card->setMod(mod.displayName, mod.type, mod.isInstalled, disabled, mod.modID, mod.hasUpdate);
        }
    });
}

void ModList::setupModGrid() {
    m_grid->setFastSkeletonMode(true);
    m_grid->setPadding(5, 0, 5, 40);
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

void ModList::submitNextCard() {
    auto& mods = m_modManager.mods();
    size_t modIdx = mods.size();
    int bestDist = INT_MAX;
    for (size_t index = 0; index < mods.size(); index++) {
        if (!mods[index].isPending) continue;
        int dist = std::abs(static_cast<int>(index) - m_focusedIndex);
        if (dist < bestDist) {
            bestDist = dist;
            modIdx = index;
        }
    }
    if (modIdx == mods.size()) {
        startMetadataLoader(true);
        return;
    }

    std::string dirName = mods[modIdx].dirName;
    auto token = m_stopSource.get_token();
    FrameQueue::enqueue(token, [this, dirName = std::move(dirName)] {
        showCard(dirName);
    });
}

void ModList::showCard(const std::string& dirName) {
    int modIdx = m_modManager.findByDirName(dirName);
    if (modIdx >= 0) {
        m_modManager.mods()[modIdx].isPending = false;
        m_grid->reloadItem(static_cast<size_t>(modIdx));
    }
    submitNextCard();
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

int ModList::finalizeModAddition(bool sortMods) {
    std::string lastDir = m_modManager.mods().back().dirName;
    if (sortMods) m_modManager.sort();
    int newIdx = m_modManager.findByDirName(lastDir);

    auto& game = m_gameManager.games()[m_gameIndex];
    m_gameManager.setModCount(m_gameIndex, static_cast<int>(m_modManager.mods().size()));
    m_gameManager.setPendingFocusPath(game.dirPath);
    return newIdx;
}

void ModList::setupDetail() {
    // 标签区域启用自动换行（Borealis XML 不支持 flexWrap 属性，直接调 Yoga API）
    YGNodeStyleSetFlexWrap(m_tagRow->getYGNode(), YGWrapWrap);

    // 焦点进入时显示描述区滚动条，离开时显示底部提示
    m_scroll->setScrollingIndicatorVisible(false);
    m_scrollHint->setVisibility(brls::Visibility::INVISIBLE);
    m_detail->getFocusEvent()->subscribe([this](brls::View*) {
        m_scroll->setScrollingIndicatorVisible(true);
        updateScrollHintVisibility();
    });
    m_detail->getFocusLostEvent()->subscribe([this](brls::View*) {
        m_scroll->setScrollingIndicatorVisible(false);
        updateScrollHintVisibility();
    });

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
    setGameIcon();

    // 收藏标志
    m_favIcon->setVisibility(game.isFavorite ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
}

void ModList::updateScrollHintVisibility() {
    bool contentOverflows = m_scrollContent->getHeight() > m_scroll->getHeight();
    auto visibility = !m_detail->isFocused() && contentOverflows ? brls::Visibility::VISIBLE : brls::Visibility::INVISIBLE;
    if (m_scrollHint->getVisibility() != visibility) m_scrollHint->setVisibility(visibility);
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
    updateScrollHintVisibility();
}

void ModList::setGameIcon() {
    auto token = m_stopSource.get_token();
    std::string tid = format::appIdHex(m_modManager.game().appId);

    FrameQueue::enqueue(token, [this, tid, token] {
        int textureId = brls::TextureCache::instance().getCache(tid);
        if (textureId > 0) {
            m_gameIcon->setFreeTexture(false);
            m_gameIcon->innerSetImage(textureId);
            return; // 命中:不调度重试,保持现有语义
        }

        m_gameIcon->setImageFromRes("img/game/defaultIcon.jpg");

        m_iconRetryDelayId = brls::delay(1000, [this, tid, token] {
            m_iconRetryDelayId = 0;
            FrameQueue::enqueue(token, [this, tid] {
                int textureId = brls::TextureCache::instance().getCache(tid);
                if (textureId == 0) return;

                brls::TextureCache::instance().removeCache(m_gameIcon->getTexture());
                m_gameIcon->innerSetImage(textureId);
            });
        });
    });
}

void ModList::startMetadataLoader(bool checkUpdatesWhenDone) {
    if (m_metadataLoading) return;
    m_metadataLoading = true;
    submitNextMetadata(checkUpdatesWhenDone);
}

void ModList::submitNextMetadata(bool checkUpdatesWhenDone) {
    auto& mods = m_modManager.mods();
    size_t modIdx = mods.size();
    int bestDist = INT_MAX;
    for (size_t index = 0; index < mods.size(); index++) {
        if (!mods[index].isMetadataPending) continue;
        int dist = std::abs(static_cast<int>(index) - m_focusedIndex);
        if (dist < bestDist) {
            bestDist = dist;
            modIdx = index;
        }
    }

    if (modIdx == mods.size()) {
        m_metadataLoading = false;
        m_modManager.saveJson();
        if (checkUpdatesWhenDone) submitUpdateCheck();
        return;
    }

    auto& mod = mods[modIdx];
    std::string dirName = mod.dirName;
    std::string modPath = mod.path;
    std::string zipPath = mod.path + "/" + mod.dirName + ".zip";
    bool needSize = mod.size.empty();
    bool needCrc = mod.modID > 0 && mod.fileCrc32.empty();
    // 提交前消费，计算失败时本次页面停留期间不再重试
    mod.isMetadataPending = false;

    auto token = m_stopSource.get_token();
    ThreadPool::instance().submit([this, dirName = std::move(dirName), modPath = std::move(modPath), zipPath = std::move(zipPath), needSize, needCrc, checkUpdatesWhenDone](std::stop_token token) {
        std::string sizeStr;
        if (needSize) {
            int64_t bytes = fs::calcDirSize(modPath, &token);
            if (token.stop_requested()) return;
            if (bytes >= 0) sizeStr = format::fileSize(bytes);
        }

        std::string crc32;
        if (needCrc) {
            std::vector<char> buf(64 * 1024);
            int64_t crc = crc::fromFile(zipPath.c_str(), buf.data(), buf.size(), &token);
            if (token.stop_requested()) return;
            if (crc >= 0) {
                char hex[9];
                std::snprintf(hex, sizeof(hex), "%08x", static_cast<uint32_t>(crc));
                crc32 = hex;
            }
        }

        brls::sync([this, dirName = std::move(dirName), sizeStr = std::move(sizeStr), crc32 = std::move(crc32), checkUpdatesWhenDone, token] {
            if (token.stop_requested()) return;
            applyMetadataResult(dirName, sizeStr, crc32);
            submitNextMetadata(checkUpdatesWhenDone);
        });
    }, token);
}

void ModList::applyMetadataResult(const std::string& dirName, const std::string& sizeStr, const std::string& crc32) {
    int modIdx = m_modManager.findByDirName(dirName);
    if (modIdx < 0) return;

    if (!sizeStr.empty()) m_modManager.setSize(modIdx, sizeStr);
    if (!crc32.empty()) m_modManager.setFileCrc32(modIdx, crc32);

    auto& mods = m_modManager.mods();
    if (!sizeStr.empty() && m_lastFocusIndex < mods.size() && mods[m_lastFocusIndex].dirName == dirName) {
        m_tagSize->setText(sizeStr);
    }
}

void ModList::submitUpdateCheck() {
    if (!deviceInfo::Network::isAvailable()) return;

    std::string modsJson = m_modManager.buildUpdateCheckJson();
    if (modsJson.empty()) return;

    std::string gameTid = format::appIdHex(m_modManager.game().appId);

    ThreadPool::instance().submit([this, gameTid = std::move(gameTid), modsJson = std::move(modsJson)](std::stop_token token) {
        auto result = api::mod::checkModUpdates(gameTid, modsJson, token);
        if (token.stop_requested()) return;
        brls::sync([this, result = std::move(result), token] {
            if (token.stop_requested()) return;
            if (!result.success) return;

            auto& mods = m_modManager.mods();
            for (int id : result.updatedModIds) {
                int idx = m_modManager.findByModID(id);
                if (idx < 0) continue;
                mods[idx].hasUpdate = true;
                m_grid->reloadItem(static_cast<size_t>(idx));
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
    m_editMenu.setIcon(format::themedIconPath("img/menu/manualInput"));
    m_typeMenu.setIcon(format::themedIconPath("img/menu/type"));

    auto& editNameItem = m_editMenu.addAction(brls::getStr("page/modList/editName"), brls::getStr("page/modList/editNameDesc"));
    editNameItem.setIcon(format::themedIconPath("img/menu/title"));
    editNameItem.setDisabled([this]{ return m_modManager.mods().empty(); });
    editNameItem.setBadge("\uE14A");
    editNameItem.setStayOpen();
    editNameItem.onSelected([this]{ manualSetModDisplayName(); });

    auto& restoreNameItem = m_editMenu.addAction(brls::getStr("page/modList/editRestoreName"), brls::getStr("page/modList/editRestoreNameDesc"));
    restoreNameItem.setIcon(format::themedIconPath("img/menu/restoreTitle"));
    restoreNameItem.setDisabled([this]{ return m_modManager.mods().empty(); });
    restoreNameItem.setBadge("\uE14A");
    restoreNameItem.setStayOpen();
    restoreNameItem.onSelected([this]{ resetModDisplayName(); });

    auto& editDescItem = m_editMenu.addAction(brls::getStr("page/modList/editDesc"), brls::getStr("page/modList/editDescDesc"));
    editDescItem.setIcon(format::themedIconPath("img/menu/manager"));
    editDescItem.setDisabled([this]{ return m_modManager.mods().empty(); });
    editDescItem.setBadge("\uE14A");
    editDescItem.setStayOpen();
    editDescItem.onSelected([this]{ editModDescription(); });

    auto& editTypeItem = m_editMenu.addSubmenu(brls::getStr("page/modList/editType"), brls::getStr("page/modList/editTypeDesc"));
    editTypeItem.setIcon(format::themedIconPath("img/menu/type"));
    editTypeItem.setDisabled([this]{ return m_modManager.mods().empty(); });
    editTypeItem.setBadge([this]{
        if (m_modManager.mods().empty()) return std::string();
        return modTypeText(m_modManager.mods()[m_focusedIndex].type);
    });
    editTypeItem.setPage(m_typeMenu);

    auto& editModVerItem = m_editMenu.addAction(brls::getStr("page/modList/editModVer"), brls::getStr("page/modList/editModVerDesc"));
    editModVerItem.setIcon(format::themedIconPath("img/menu/hVerson"));
    editModVerItem.setDisabled([this]{ return m_modManager.mods().empty(); });
    editModVerItem.setBadge([this]{
        if (m_modManager.mods().empty()) return std::string();
        auto& v = m_modManager.mods()[m_focusedIndex].modVersion;
        return v.empty() ? brls::getStr("page/modList/unknown") : v;
    });
    editModVerItem.setStayOpen();
    editModVerItem.onSelected([this]{ editModVersion(); });

    auto& editGameVerItem = m_editMenu.addAction(brls::getStr("page/modList/editGameVer"), brls::getStr("page/modList/editGameVerDesc"));
    editGameVerItem.setIcon(format::themedIconPath("img/menu/common"));
    editGameVerItem.setDisabled([this]{ return m_modManager.mods().empty(); });
    editGameVerItem.setBadge([this]{
        if (m_modManager.mods().empty()) return std::string();
        auto& v = m_modManager.mods()[m_focusedIndex].gameVersion;
        if (v.empty()) return brls::getStr("page/modList/unknown");
        if (v == "0") return brls::getStr("page/modList/versionUniversal");
        return v;
    });
    editGameVerItem.setStayOpen();
    editGameVerItem.onSelected([this]{ editGameVersion(); });

    auto& editAuthorItem = m_editMenu.addAction(brls::getStr("page/modList/editAuthor"), brls::getStr("page/modList/editAuthorDesc"));
    editAuthorItem.setIcon(format::themedIconPath("img/menu/userNanme"));
    editAuthorItem.setDisabled([this]{ return m_modManager.mods().empty(); });
    editAuthorItem.setBadge([this]{
        if (m_modManager.mods().empty()) return std::string();
        auto& v = m_modManager.mods()[m_focusedIndex].author;
        return v.empty() ? brls::getStr("page/modList/unknown") : v;
    });
    editAuthorItem.setStayOpen();
    editAuthorItem.onSelected([this]{ editModAuthor(); });

    // ── 类型选择子菜单 ──
    for (auto& opt : modTypeOptions()) {
        std::string val = opt.value;
        auto& item = m_typeMenu.addRadio(opt.label, opt.desc);
        item.setSelected([this, val]{
            if (m_modManager.mods().empty()) return false;
            return m_modManager.mods()[m_focusedIndex].type == val;
        });
        item.onSelected([this, val]{ applyModType(m_focusedIndex, val); });
        item.setBack();
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
                        m_modManager.clearAllInstalledStates();
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

void ModList::addModsFromTransit() {
    auto mods = ModManager::scanTransitMods();
    if (mods.empty()) {
        CustomDialog::show(brls::getStr("page/modList/transitEmpty"), {{brls::getStr("page/modList/ok"), [] { CustomDialog::close(); }}});
        return;
    }

    ContextMultiSelectPage menu(brls::getStr("page/modList/selectMod"));
    for (const auto& mod : mods) {
        menu.addOption(mod.name, "");
    }

    menu.onConfirm([this, mods = std::move(mods)](const std::vector<int>& selected) mutable {
        CustomDialog::show(brls::getStr("page/modList/addModConfirm", std::to_string(selected.size())), {
            {brls::getStr("page/modList/cancel"), [] { CustomDialog::close(); }},
            {brls::getStr("page/modList/confirm"), [this, mods = std::move(mods), selected] {
                std::vector<fs::DirEntry> chosen;
                for (int i : selected) chosen.push_back(mods[i]);

                int added = m_modManager.addModsFormTransitForModList(chosen);
                int newIdx = added > 0 ? finalizeModAddition(false) : -1;

                auto onClose = [this, added, newIdx] {
                    CustomDialog::close([this, added, newIdx] {
                        if (added <= 0) return;
                        refreshAndFocus(newIdx);
                        startMetadataLoader(false);
                    });
                };
                CustomDialog::show(brls::getStr("page/modList/addModSuccess", std::to_string(added)), {{brls::getStr("page/modList/ok"), onClose}}, onClose);
            }},
        });
    });

    menu.show();
}

void ModList::removeModFromList() {
    int idx = m_focusedIndex;

    auto onConfirm = [this, idx] {
        m_modManager.removeModFromModList(idx);

        auto& game = m_gameManager.games()[m_gameIndex];
        m_gameManager.setModCount(m_gameIndex, static_cast<int>(m_modManager.mods().size()));
        m_gameManager.setPendingFocusPath(game.dirPath);

        int newFocus = std::min(idx, static_cast<int>(m_modManager.mods().size()) - 1);
        CustomDialog::close([this, newFocus] { refreshAndFocus(newFocus); });
    };

    CustomDialog::show(brls::getStr("page/modList/removeModConfirm"), {
        {brls::getStr("page/modList/cancel"), [] { CustomDialog::close(); }},
        {brls::getStr("page/modList/cancel"), [] { CustomDialog::close(); }},
        {brls::getStr("page/modList/removeModBtn"), onConfirm},
    });
}

void ModList::deleteModFromList() {
    int idx = m_focusedIndex;

    auto onConfirm = [this, idx] { startDeleteMod(idx); };

    CustomDialog::show(brls::getStr("page/modList/deleteModConfirm"), {
        {brls::getStr("page/modList/cancel"), [] { CustomDialog::close(); }},
        {brls::getStr("page/modList/cancel"), [] { CustomDialog::close(); }},
        {brls::getStr("page/modList/deleteModBtn"), onConfirm},
    });
}

void ModList::startDeleteMod(int idx) {
    bool lastMod = m_modManager.mods().size() == 1;
    std::string gameDirPath = m_gameManager.games()[m_gameIndex].dirPath;

    deviceControl::HomeButton::disable();
    ProgressDialog::show(brls::getStr("page/modList/deletingMod"), {}, [] {});

    m_installTask = ThreadPool::instance().submitWaitable([this, idx, lastMod, gameDirPath](std::stop_token) {
        auto result = m_modManager.deleteModContents(idx, [](int deleted, int total, const char* fileName) {
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

        // 等待一段时间，避免因删除太快，导致进度框快速切换，视觉闪烁
        for (int i = 0; i < 30; i++) svcSleepThread(10000000ULL);  // 10ms × 30 = 300ms

        brls::sync([this, idx, lastMod, gameDirPath, result] {
            if (result.status != fs::RemoveResult::Completed) {
                deviceControl::HomeButton::enable();
                CustomDialog::show(brls::getStr("page/modList/deleteModFailed", result.errorPath, result.deletedCount, result.totalCount, result.errorCode), {{brls::getStr("page/modList/ok"), [] { CustomDialog::close(); }}});
                return;
            }

            m_modManager.deleteModFromModList(idx);
            int newFocus = 0;
            if (lastMod) {
                m_gameManager.setPendingCleanup(gameDirPath);
            } else {
                m_gameManager.setModCount(m_gameIndex, static_cast<int>(m_modManager.mods().size()));
                m_gameManager.setPendingFocusPath(gameDirPath);
                newFocus = std::min(idx, static_cast<int>(m_modManager.mods().size()) - 1);
            }

            ProgressDialog::close([this, lastMod, newFocus] {
                deviceControl::HomeButton::enable();
                if (lastMod) Page::popPage();
                else refreshAndFocus(newFocus);
            });
        });
    }, std::stop_token{});
}

void ModList::removeLastModFromList() {
    int idx = m_focusedIndex;

    auto onConfirm = [this, idx] {
        m_modManager.removeModFromModList(idx);

        auto& game = m_gameManager.games()[m_gameIndex];
        m_gameManager.setPendingCleanup(game.dirPath);
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

void ModList::deleteLastModFromList() {
    int idx = m_focusedIndex;

    auto onConfirm = [this, idx] { startDeleteMod(idx); };

    CustomDialog::show(brls::getStr("page/modList/deleteLastModConfirm"), {
        {brls::getStr("page/modList/cancel"), [] { CustomDialog::close(); }},
        {brls::getStr("page/modList/cancel"), [] { CustomDialog::close(); }},
        {brls::getStr("page/modList/deleteModBtn"), onConfirm},
    });
}

void ModList::setupMenu() {
    setupEditMenu();
    setupLocalAddMenu();
    setupAssistFeaturesMenu();

    // ── 主菜单 ──
    m_menu.setIcon(format::themedIconPath("img/menu/menu"));
    m_menu.setShowFakeHighlight([this]{ return !m_modManager.mods().empty(); });

    auto& editSubmenuItem = m_menu.addSubmenu(brls::getStr("page/modList/editSubmenu"), brls::getStr("page/modList/editSubmenuDesc"));
    editSubmenuItem.setIcon(format::themedIconPath("img/menu/manualInput"));
    editSubmenuItem.setDisabled([this]{ return m_modManager.mods().empty(); });
    editSubmenuItem.setBadge("\uE14A");
    editSubmenuItem.setPage(m_editMenu);

    auto& localAddItem = m_menu.addSubmenu(brls::getStr("page/modList/localAdd"), brls::getStr("page/modList/localAddDesc"));
    localAddItem.setIcon(format::themedIconPath("img/menu/add"));
    localAddItem.setBadge("\uE14A");
    localAddItem.setPage(m_localAddMenu);

    auto& removeModItem = m_menu.addAction(brls::getStr("page/modList/removeMod"), brls::getStr("page/modList/removeModDesc"));
    removeModItem.setIcon(format::themedIconPath("img/menu/removeItem"));
    removeModItem.setDisabled([this]{
        if (m_modManager.mods().empty()) return true;
        auto idx = m_focusedIndex;
        if (idx >= static_cast<int>(m_modManager.mods().size())) return true;
        if (m_modManager.mods()[idx].isInstalled) return true;
        return m_metadataLoading;
    });
    removeModItem.setBadge("\uE14A");
    removeModItem.onSelected([this]{
        if (m_modManager.mods().size() == 1) removeLastModFromList();
        else removeModFromList();
    });

    auto& deleteModItem = m_menu.addAction(brls::getStr("page/modList/deleteMod"), brls::getStr("page/modList/deleteModDesc"));
    deleteModItem.setIcon(format::themedIconPath("img/menu/clearTransferStation"));
    deleteModItem.setDisabled([this]{
        if (m_modManager.mods().empty()) return true;
        auto idx = m_focusedIndex;
        if (idx >= static_cast<int>(m_modManager.mods().size())) return true;
        if (m_modManager.mods()[idx].isInstalled) return true;
        return m_metadataLoading;
    });
    deleteModItem.setBadge("\uE14A");
    deleteModItem.onSelected([this]{
        if (m_modManager.mods().size() == 1) deleteLastModFromList();
        else deleteModFromList();
    });

    auto& viewPathItem = m_menu.addAction(brls::getStr("page/modList/viewPath"), brls::getStr("page/modList/viewPathDesc"));
    viewPathItem.setIcon(format::themedIconPath("img/menu/viewLocation"));
    viewPathItem.setDisabled([this]{ return m_modManager.mods().empty(); });
    viewPathItem.setBadge("\uE14A");
    viewPathItem.onSelected([this]{
        auto& mod = m_modManager.mods()[m_focusedIndex];
        CustomDialog::show(mod.path, {{brls::getStr("page/modList/ok"), [] { CustomDialog::close(); }}});
    });

    auto& viewAuthorItem = m_menu.addAction(brls::getStr("page/modList/viewAuthor"), brls::getStr("page/modList/viewAuthorDesc"));
    viewAuthorItem.setIcon(format::themedIconPath("img/menu/userNanme"));
    viewAuthorItem.setDisabled([this]{
        return m_modManager.mods().empty() || m_modManager.mods()[m_focusedIndex].authorLink.empty();
    });
    viewAuthorItem.setBadge("\uE14A");
    viewAuthorItem.onSelected([this]{
        QrCodeView::show(m_modManager.mods()[m_focusedIndex].authorLink);
    });

    auto& assistFeaturesItem = m_menu.addSubmenu(brls::getStr("page/modList/assistFeatures"), brls::getStr("page/modList/assistFeaturesDesc"));
    assistFeaturesItem.setIcon(format::themedIconPath("img/menu/otherF"));
    assistFeaturesItem.setDisabled([this]{ return m_modManager.mods().empty(); });
    assistFeaturesItem.setBadge("\uE14A");
    assistFeaturesItem.setPage(m_assistFeaturesMenu);
}

void ModList::openStoreModDetail() {
    if (m_modManager.mods().empty() ||
        m_focusedIndex >= static_cast<int>(m_modManager.mods().size()) ||
        m_modManager.mods()[m_focusedIndex].modID < 0) {
        CustomDialog::show(brls::getStr("page/modList/storeModNotListed"), {{brls::getStr("page/modList/ok"), [] { CustomDialog::close(); }}});
        return;
    }

    auto& mod = m_modManager.mods()[m_focusedIndex];
    auto& game = m_gameManager.games()[m_gameIndex];
    std::string tid = format::appIdHex(game.appId);
    Page::pushPage(new StoreModDetail(mod.modID, tid, game.displayName, tid, m_gameManager, nullptr, &m_modManager, game.version, true, true), PageAnimType::SlideFromRight);
}

void ModList::setupLocalAddMenu() {
    m_localAddMenu.setIcon(format::themedIconPath("img/menu/add"));

    auto& selectModItem = m_localAddMenu.addAction(brls::getStr("page/modList/selectMod"), brls::getStr("page/modList/selectModDesc"));
    selectModItem.setIcon(format::themedIconPath("img/menu/multipleSelection"));
    selectModItem.setBadge([]{ int n = ModManager::transitModCount(); return std::to_string(n) + " MOD"; });
    selectModItem.onSelected([this]{ addModsFromTransit(); });

    auto& mtpItem = m_localAddMenu.addAction(brls::getStr("page/modList/mtpTransfer"), brls::getStr("page/modList/mtpTransferDesc"));
    mtpItem.setIcon(format::themedIconPath("img/menu/mtp"));
    mtpItem.setBadge(brls::getStr("page/addGame/menuMtpBadge"));
    mtpItem.onSelected([] {
        std::vector<MtpMount> mounts = {
            {"/mods2/!temp_mods", "Add Mod"},
        };
        mtpDialog::open(mounts);
    });

    auto& ftpItem = m_localAddMenu.addAction(brls::getStr("page/modList/ftpTransfer"), brls::getStr("page/modList/ftpTransferDesc"));
    ftpItem.setIcon(format::themedIconPath("img/menu/ftp"));
    ftpItem.setBadge(brls::getStr("page/addGame/menuFtpBadge"));
    ftpItem.onSelected([] {
        std::vector<FtpMount> mounts = {
            {"/mods2/!temp_mods", "Add Mod"},
        };
        ftpDialog::open(mounts);
    });
}

void ModList::setupAssistFeaturesMenu() {
    m_assistFeaturesMenu.setIcon(format::themedIconPath("img/menu/otherF"));

    auto& forceCleanItem = m_assistFeaturesMenu.addAction(brls::getStr("page/modList/forceClean"), brls::getStr("page/modList/forceCleanDesc"));
    forceCleanItem.setIcon(format::themedIconPath("img/menu/clearTransferStation"));
    forceCleanItem.setDisabled([this]{ return m_modManager.mods().empty(); });
    forceCleanItem.setBadge("\uE14A");
    forceCleanItem.onSelected([this]{ forceClean(); });

    auto& disableItem = m_assistFeaturesMenu.addSwitch(brls::getStr("page/modList/disableMod"), brls::getStr("page/modList/disableModDesc"));
    disableItem.setIcon(format::themedIconPath("img/menu/notInstalled"));
    disableItem.setDisabled([this]{ return m_modManager.mods().empty(); });
    disableItem.setState([this]{ return m_modManager.game().isModsDisabled; });
    disableItem.setTask([this](bool) { toggleModDisable(); });
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

    finalizeModAddition(true);

    ScrollDialog::close([this] {
        refreshAndFocus(0);
        submitNextCard();
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
