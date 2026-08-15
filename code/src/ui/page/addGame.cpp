/**
 * AddGame - 添加游戏页面
 */

#include "ui/page/addGame.hpp"
#include "common/config.hpp"
#include "common/settings.hpp"
#include "core/audio.hpp"
#include "core/frameQueue.hpp"
#include "core/modManager.hpp"
#include "ui/dataSource/installedGameDS.hpp"
#include "ui/navigation/navigationGroups.hpp"
#include "ui/page/search.hpp"
#include "ui/view/addGameCard.hpp"
#include "ui/view/dialog/customDialog.hpp"
#include "ui/view/dialog/ftpDialog.hpp"
#include "ui/view/dialog/keyboardInput.hpp"
#include "ui/view/dialog/longPressDialog.hpp"
#include "ui/view/dialog/mtpDialog.hpp"
#include "ui/view/dialog/scrollDialog.hpp"
#include "ui/view/longTextBox.hpp"
#include "utils/format.hpp"
#include "utils/threadPool.hpp"
#include <borealis/core/cache_helper.hpp>
#include <borealis/core/i18n.hpp>
#include <algorithm>
#include <climits>
#include <cstdlib>
#include <utility>

AddGame::AddGame(GameManager& gameManager)
    : m_gameManager(gameManager) {
    prepareGames();
    inflateFromXMLRes("xml/view/page/addGame.xml");

    setHeader();
}

void AddGame::prepareGames() {
    auto& games = m_gameManager.getInstalledGames();
    m_gameManager.sortInstalledGames(true);

    for (auto& game : games) game.isPending = true;
}

void AddGame::setHeader() {
    HeaderState headerState;
    headerState.setNavigation(createMainNavigationState(MainNavigationPage::AddGame));
    ShellState::setHeaderState(headerState);
}

AddGame::~AddGame() {
    m_stopSource.request_stop();
}

void AddGame::onContentAvailable() {
    // B 键：退出（Home::onResume 会自动检查 pendingFocus 并刷新）
    registerAction("", brls::BUTTON_B, [this](brls::View*) {
        Audio::instance()->play(SoundEffect::Enter);
        Page::popPage(PageAnimType::SlideFromLeft);
        return true;
    }, true);

    // ZL：返回主页（隐藏 hint）
    registerAction("", brls::BUTTON_LT, [this](...) {
        Audio::instance()->play(SoundEffect::Enter);
        Page::popPage(PageAnimType::SlideFromLeft);
        return true;
    }, true);

    // ZR：导航右边界
    registerAction("", brls::BUTTON_RT, [this](...) {
        Audio::instance()->play(SoundEffect::FocusLimit);
        shakeHeaderNav(true);
        return true;
    }, true);

    // - 键：打开搜索页面
    registerAction(brls::getStr("page/addGame/search"), brls::BUTTON_BACK, [this](...) {
        Audio::instance()->play(SoundEffect::Enter);
        std::vector<std::string> names;
        for (auto& game : m_gameManager.getInstalledGames()) {
            names.push_back(game.displayName);
        }
        Page::pushPage(new Search(names, [this](int index) {
            m_grid->selectRowAt(index, false);
            m_grid->instantFocus(index);
        }));
        return true;
    });

    // Y 键：切换排序方向
    registerAction(brls::getStr("page/addGame/sortAsc"), brls::BUTTON_Y, [this](...) {
        Audio::instance()->play(SoundEffect::Click);
        toggleSort();
        return true;
    });
    setActionAvailable(brls::BUTTON_BACK, false);

    // X 键：打开添加模组菜单
    setupMenu();
    registerAction(brls::getStr("page/addGame/menu"), brls::BUTTON_X, [this](...) {
        Audio::instance()->play(SoundEffect::Enter);
        m_addModMenu.show();
        return true;
    });

    setupGridPage();
    submitNextCard();
    runFirstLaunchDialog();
}

void AddGame::setupMenu() {
    m_addModMenu.setIcon(format::themedIconPath("img/menu/add"));

    auto& mtpItem = m_addModMenu.addAction(brls::getStr("page/addGame/menuMtp"), brls::getStr("page/addGame/menuMtpDesc"));
    mtpItem.setIcon(format::themedIconPath("img/menu/mtp"));
    mtpItem.setBadge(brls::getStr("page/addGame/menuMtpBadge"));
    mtpItem.onSelected([] {
        std::vector<MtpMount> mounts = {
            {"/mods2/!temp_mods", "Add Mod"},
        };
        mtpDialog::open(mounts);
    });

    auto& ftpItem = m_addModMenu.addAction(brls::getStr("page/addGame/menuFtp"), brls::getStr("page/addGame/menuFtpDesc"));
    ftpItem.setIcon(format::themedIconPath("img/menu/ftp"));
    ftpItem.setBadge(brls::getStr("page/addGame/menuFtpBadge"));
    ftpItem.onSelected([] {
        std::vector<FtpMount> mounts = {
            {"/mods2/!temp_mods", "Add Mod"},
        };
        ftpDialog::open(mounts);
    });
}

void AddGame::onGameCardClicked(size_t index) {
    auto mods = ModManager::scanTransitMods();
    if (mods.empty()) {
        ScrollDialog::show(createTransitEmptyBox(), brls::getStr("page/addGame/cancel"), [] { ScrollDialog::close(); }, brls::getStr("page/addGame/confirm"), [] { ScrollDialog::close(); }, [] { ScrollDialog::close(); });
        return;
    }

    if (index == 0) onVirtualGameCardClicked(std::move(mods));
    else onRealGameCardClicked(index, std::move(mods));
}

brls::Box* AddGame::createTransitEmptyBox() {
    LongTextBoxConfig content;

    auto& method = content.addEntry();
    method.addTitle(brls::getStr("page/addGame/transitEmptyTitle"));
    method.addBody(brls::getStr("page/addGame/firstLaunchMethodBody"));

    auto& format = content.addEntry();
    format.addTitle(brls::getStr("page/addGame/transitEmptyFormatTitle"));
    format.addBody(brls::getStr("page/addGame/firstLaunchFormatBody"));

    auto& notice = content.addEntry();
    notice.addTitle(brls::getStr("page/addGame/transitEmptyNoticeTitle"));
    notice.addBody(brls::getStr("page/addGame/firstLaunchFilenameBody"));

    return LongTextBox::create(content);
}

void AddGame::onRealGameCardClicked(size_t index, std::vector<fs::DirEntry> mods) {
    ContextMultiSelectPage menu(brls::getStr("page/addGame/selectMod"));
    menu.setIcon(format::themedIconPath("img/menu/multipleSelection"));
    for (const auto& mod : mods) {
        auto& option = menu.addOption(mod.name, "");
        std::string modPath = std::string(config::transitDir) + mod.name;
        bool isZip = !fs::listSubFiles(modPath, config::modFileExts).empty();
        option.setIcon(format::themedIconPath(isZip ? "img/menu/zip" : "img/menu/folder"));
    }
    menu.onConfirm([this, index, mods = std::move(mods)](const std::vector<int>& selected) mutable {
        auto onConfirmAdd = [this, index, mods = std::move(mods), selected] {
            std::vector<fs::DirEntry> chosen;
            for (int i : selected) chosen.push_back(mods[i]);
            auto& installed = m_gameManager.getInstalledGames()[index];
            int iconId = -1;
            if (m_gameManager.findByAppId(installed.appId) < 0 && !installed.iconKey.empty()) {
                int cachedIconId = brls::TextureCache::instance().getCache(installed.iconKey);
                if (cachedIconId > 0) iconId = cachedIconId;
            }
            std::string dirPath = m_gameManager.addGame(index, static_cast<int>(chosen.size()), iconId);
            int added = ModManager::addModsFormTransit(dirPath, chosen);
            m_gameManager.setPendingFocusPath(dirPath);
            auto* cell = m_grid->getGridItemByIndex(index);
            if (cell) static_cast<AddGameCard*>(cell)->setGame(installed.displayName, installed.version, installed.modCount);
            CustomDialog::show(brls::getStr("page/addGame/addSuccess", added), {
                {brls::getStr("page/addGame/continueAdd"), [] { CustomDialog::close(); }},
                {brls::getStr("page/addGame/backToHome"), [this] {
                    CustomDialog::close([this] { Page::popPage(PageAnimType::SlideFromLeft); });
                }},
            });
        };

        CustomDialog::show(brls::getStr("page/addGame/addConfirm", selected.size()), {
            {brls::getStr("page/addGame/cancel"), [] { CustomDialog::close(); }},
            {brls::getStr("page/addGame/confirm"), onConfirmAdd},
        });
    });
    menu.show();
}

void AddGame::onVirtualGameCardClicked(std::vector<fs::DirEntry> mods) {
    auto onTidConfirm = [this, mods = std::move(mods)](std::string tid) mutable {
        uint64_t appId = format::appIdFromHex(tid);
        if (!format::appIdIsValid(appId)) {
            CustomDialog::show(brls::getStr("page/addGame/tidInvalid"), {{brls::getStr("page/addGame/ok"), [] { CustomDialog::close(); }}});
            return;
        }

        int installedIdx = m_gameManager.findInstalledByAppId(appId);
        if (installedIdx >= 0) {
            onRealGameCardClicked(static_cast<size_t>(installedIdx), std::move(mods));
            return;
        }

        // 虚拟游戏：Switch 上未安装，照抄模组选择流程
        ContextMultiSelectPage menu(brls::getStr("page/addGame/selectMod"));
        menu.setIcon(format::themedIconPath("img/menu/multipleSelection"));
        for (const auto& mod : mods) {
            auto& option = menu.addOption(mod.name, "");
            std::string modPath = std::string(config::transitDir) + mod.name;
            bool isZip = !fs::listSubFiles(modPath, config::modFileExts).empty();
            option.setIcon(format::themedIconPath(isZip ? "img/menu/zip" : "img/menu/folder"));
        }
        menu.onConfirm([this, tid, mods = std::move(mods)](const std::vector<int>& selected) mutable {
            auto onConfirmAdd = [this, tid, mods = std::move(mods), selected] {
                std::vector<fs::DirEntry> chosen;
                for (int i : selected) chosen.push_back(mods[i]);
                std::string dirPath = m_gameManager.addGameByTid(tid, static_cast<int>(chosen.size()));
                int added = ModManager::addModsFormTransit(dirPath, chosen);
                m_gameManager.setPendingFocusPath(dirPath);
                CustomDialog::show(brls::getStr("page/addGame/addSuccess", added), {
                    {brls::getStr("page/addGame/continueAdd"), [] { CustomDialog::close(); }},
                    {brls::getStr("page/addGame/backToHome"), [this] { CustomDialog::close([this] { Page::popPage(PageAnimType::SlideFromLeft); });}},
                });
            };
            CustomDialog::show(brls::getStr("page/addGame/addConfirm", selected.size()), {
                {brls::getStr("page/addGame/cancel"), [] { CustomDialog::close(); }},
                {brls::getStr("page/addGame/confirm"), onConfirmAdd}
            });
        });
        menu.show();
    };
    KeyboardInput::show(onTidConfirm, brls::getStr("page/addGame/tidPlaceholder"), 16);
}

void AddGame::setupGridPage() {
    m_grid->setPadding(5, 15, 5, 40);
    m_grid->registerCell("AddGameCard", AddGameCard::create);

    auto onTextureMissing = [this](size_t index) { queueCardReload(index); };
    auto onGameSelected = [this](size_t index) { onGameCardClicked(index); };
    auto* dataSource = new InstalledGameDS(m_gameManager.getInstalledGames(), onTextureMissing, onGameSelected);
    m_grid->setDataSource(dataSource);

    m_grid->setFocusChangeCallback([this](size_t index) {
        m_focusedIndex = static_cast<int>(index);
        ShellState::setIndexText(std::to_string(index + 1) + " / " + std::to_string(m_gameManager.getInstalledGames().size()));
    });
}

void AddGame::queueCardReload(size_t gameIdx) {
    auto& games = m_gameManager.getInstalledGames();
    bool startLoader = std::none_of(games.begin(), games.end(), [](const InstalledGameInfo& game) { return game.isPending; });
    games[gameIdx].isLoaded = false;
    games[gameIdx].isPending = true;
    if (startLoader) submitNextCard();
}

void AddGame::enableSearch() {
    if (m_searchReady) return;
    m_searchReady = true;
    setActionAvailable(brls::BUTTON_BACK, true);
}

void AddGame::submitNextCard() {
    auto& games = m_gameManager.getInstalledGames();

    // 找离焦点最近的待显示游戏
    size_t gameIdx = games.size();
    int bestDist = INT_MAX;
    for (size_t index = 0; index < games.size(); index++) {
        if (!games[index].isPending) continue;
        int dist = std::abs(static_cast<int>(index) - m_focusedIndex);
        if (dist < bestDist) {
            bestDist = dist;
            gameIdx = index;
        }
    }
    if (gameIdx == games.size()) {
        enableSearch();
        return;
    }

    uint64_t appId = games[gameIdx].appId;
    bool wasLoaded = games[gameIdx].isLoaded;
    auto token = m_stopSource.get_token();
    ThreadPool::instance().submit([this, appId, wasLoaded, &gameManager = m_gameManager](std::stop_token token) {
        std::string name;
        std::string version;
        imageDecoder::DecodedImage image;

        if (!wasLoaded && appId != 0) {
            auto meta = gameManager.fetchMetadataByAppId(appId);
            image = imageDecoder::decodeJpeg(meta.icon.data(), meta.icon.size());
            name = std::move(meta.name);
            version = std::move(meta.version);
        }

        if (token.stop_requested()) return;
        FrameQueue::enqueue(token, [this, appId, wasLoaded, name = std::move(name), version = std::move(version), image = std::move(image)]() mutable {
            applyCard(appId, wasLoaded, std::move(name), std::move(version), std::move(image));
        });
    }, token);
}

int AddGame::loadVirtualIcon() {
    auto& textureCache = brls::TextureCache::instance();
    int iconId = textureCache.getCache(config::virtualGameIconKey);
    if (iconId > 0) return iconId;

    auto* resourceImage = new brls::Image();
    resourceImage->setImageFromRes(config::virtualGameIconResource);
    iconId = textureCache.getCache(config::virtualGameIconKey);
    delete resourceImage;
    return iconId;
}

int AddGame::loadGameIcon(const std::string& key, const imageDecoder::DecodedImage& image) {
    auto& textureCache = brls::TextureCache::instance();
    int iconId = textureCache.getCache(key);
    if (iconId > 0 || image.width <= 0 || image.height <= 0 || image.pixels.empty()) return iconId;

    iconId = nvgCreateImageRGBA(brls::Application::getNVGContext(), image.width, image.height, 0, image.pixels.data());
    if (iconId > 0) textureCache.addCache(key, iconId);
    return iconId;
}

void AddGame::applyCard(uint64_t appId, bool wasLoaded, std::string name, std::string version, imageDecoder::DecodedImage image) {
    size_t gameIdx = static_cast<size_t>(m_gameManager.findInstalledByAppId(appId));
    auto& game = m_gameManager.getInstalledGames()[gameIdx];
    auto& textureCache = brls::TextureCache::instance();
    int iconId = 0;

    if (game.appId == 0) {
        iconId = loadVirtualIcon();
        game.iconKey = iconId > 0 ? config::virtualGameIconKey : "";
    } else if (wasLoaded) {
        if (!game.iconKey.empty()) {
            iconId = textureCache.getCache(game.iconKey);
            if (iconId <= 0) {
                game.isLoaded = false;
                submitNextCard();
                return;
            }
        }
    } else {
        std::string key = game.iconKey.empty() ? format::appIdHex(game.appId) : game.iconKey;
        iconId = loadGameIcon(key, image);
        game.iconKey = iconId > 0 ? std::move(key) : "";
    }

    if (!wasLoaded) {
        if (!name.empty()) game.displayName = std::move(name);
        if (!version.empty()) game.version = std::move(version);
        game.isLoaded = true;
    }
    showCard(gameIdx);
    if (iconId > 0) textureCache.removeCache(iconId);
}

void AddGame::showCard(size_t gameIdx) {
    m_gameManager.getInstalledGames()[gameIdx].isPending = false;
    m_grid->reloadItem(gameIdx);
    submitNextCard();
}

brls::Box* AddGame::createFirstLaunchBox() {
    LongTextBoxConfig content;

    auto& notice = content.addEntry();
    notice.addTitle(brls::getStr("page/addGame/firstLaunchNoticeTitle"));
    notice.addBody(brls::getStr("page/addGame/firstLaunchNoticeBody"));

    auto& method = content.addEntry();
    method.addTitle(brls::getStr("page/addGame/firstLaunchMethodTitle"));
    method.addBody(brls::getStr("page/addGame/firstLaunchMethodBody"));

    auto& format = content.addEntry();
    format.addTitle(brls::getStr("page/addGame/firstLaunchFormatTitle"));
    format.addBody(brls::getStr("page/addGame/firstLaunchFormatBody"));

    auto& filename = content.addEntry();
    filename.addTitle(brls::getStr("page/addGame/firstLaunchFilenameTitle"));
    filename.addBody(brls::getStr("page/addGame/firstLaunchFilenameBody"));

    return LongTextBox::create(content);
}

void AddGame::runFirstLaunchDialog() {
    if (!Settings::getBool("addGame", "firstLaunch", true)) return;
    brls::sync([this]() {
        auto onConfirm = [this]() {
            Settings::setBool("addGame", "firstLaunch", false);
            LongPressDialog::close();
        };
        LongPressDialog::show(createFirstLaunchBox(), brls::getStr("page/addGame/firstLaunchBtn"), 5.0f, onConfirm);
    });
}

void AddGame::toggleSort() {
    m_sortAsc = !m_sortAsc;
    m_gameManager.sortInstalledGames(m_sortAsc);
    m_grid->setDefaultCellFocus(0);
    m_grid->reloadData();
    m_grid->instantFocus(0);
    updateActionHint(brls::BUTTON_Y, m_sortAsc ? brls::getStr("page/addGame/sortAsc") : brls::getStr("page/addGame/sortDesc"));
    brls::Application::getGlobalHintsUpdateEvent()->fire();
}
