/**
 * AddGame - 添加游戏页面
 */

#include "ui/page/addGame.hpp"
#include "common/settings.hpp"
#include "core/audio.hpp"
#include "core/modManager.hpp"
#include "ui/dataSource/installedGameDS.hpp"
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
#include <climits>
#include <cstdlib>
#include <utility>

AddGame::AddGame(GameManager& gameManager)
    : m_gameManager(gameManager) {
    inflateFromXMLRes("xml/view/page/addGame.xml");

    ShellState::setTitle(brls::getStr("page/addGame/pageTitle"));
}

AddGame::~AddGame() {
    m_stopSource.request_stop();
}

void AddGame::onContentAvailable() {
    m_gameManager.sortInstalledGames(true);

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

    // Y 键：切换排序方向（NACP 加载完成前禁用）
    registerAction(brls::getStr("page/addGame/sortAsc"), brls::BUTTON_Y, [this](...) {
        Audio::instance()->play(SoundEffect::Click);
        toggleSort();
        return true;
    });
    setNacpActionsAvailable(false);

    // X 键：打开添加模组菜单
    setupMenu();
    registerAction(brls::getStr("page/addGame/menu"), brls::BUTTON_X, [this](...) {
        Audio::instance()->play(SoundEffect::Enter);
        m_addModMenu.show();
        return true;
    });

    loadVirtualGameIcon();
    setupGridPage();
    startNacpLoader();
    runFirstLaunchDialog();
}

void AddGame::loadVirtualGameIcon() {
    auto* image = new brls::Image();
    image->setImageFromRes("img/game/addGame.png");
    int iconId = image->getTexture();
    delete image;

    if (iconId > 0) m_gameManager.getInstalledGames()[0].iconId = iconId;
}

void AddGame::setupMenu() {
    m_addModMenu.title = brls::getStr("page/addGame/menuTitle");

    auto& mtpItem = m_addModMenu.addItem(brls::getStr("page/addGame/menuMtp"), brls::getStr("page/addGame/menuMtpDesc"));
    mtpItem.setAction([] {
        std::vector<MtpMount> mounts = {
            {"/mods2/!temp_mods", "Add Mod"},
        };
        mtpDialog::open(mounts);
    });

    auto& ftpItem = m_addModMenu.addItem(brls::getStr("page/addGame/menuFtp"), brls::getStr("page/addGame/menuFtpDesc"));
    ftpItem.setAction([] {
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
    auto* menu = new MenuPageConfig();
    menu->title = brls::getStr("page/addGame/selectMod");
    menu->multiSelect = true;
    for (const auto& mod : mods) {
        menu->addItem(mod.name);
    }
    menu->onConfirm = [this, index, mods = std::move(mods)](const std::vector<int>& selected) mutable {
        auto onConfirmAdd = [this, index, mods = std::move(mods), selected] {
            std::vector<fs::DirEntry> chosen;
            for (int i : selected) chosen.push_back(mods[i]);
            std::string dirPath = m_gameManager.addGame(index, static_cast<int>(chosen.size()));
            int added = ModManager::addModsFormTransit(dirPath, chosen);
            auto& installed = m_gameManager.getInstalledGames()[index];
            m_gameManager.setPendingFocus(installed.appId);
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
    };
    menu->show();
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
        auto* menu = new MenuPageConfig();
        menu->title = brls::getStr("page/addGame/selectMod");
        menu->multiSelect = true;
        for (const auto& mod : mods) menu->addItem(mod.name);
        menu->onConfirm = [this, tid, mods = std::move(mods)](const std::vector<int>& selected) mutable {
            auto onConfirmAdd = [this, tid, mods = std::move(mods), selected] {
                std::vector<fs::DirEntry> chosen;
                for (int i : selected) chosen.push_back(mods[i]);
                std::string dirPath = m_gameManager.addGameByTid(tid, static_cast<int>(chosen.size()));
                int added = ModManager::addModsFormTransit(dirPath, chosen);
                uint64_t appId = format::appIdFromHex(tid);
                m_gameManager.setPendingFocus(appId);
                CustomDialog::show(brls::getStr("page/addGame/addSuccess", added), {
                    {brls::getStr("page/addGame/continueAdd"), [] { CustomDialog::close(); }},
                    {brls::getStr("page/addGame/backToHome"), [this] { CustomDialog::close([this] { Page::popPage(PageAnimType::SlideFromLeft); });}},
                });
            };
            CustomDialog::show(brls::getStr("page/addGame/addConfirm", selected.size()), {
                {brls::getStr("page/addGame/cancel"), [] { CustomDialog::close(); }},
                {brls::getStr("page/addGame/confirm"), onConfirmAdd}
            });
        };
        menu->show();
    };
    KeyboardInput::show(onTidConfirm, brls::getStr("page/addGame/tidPlaceholder"), 16);
}

void AddGame::setupGridPage() {
    m_grid->setPadding(17, 15, 17, 40);
    m_grid->registerCell("AddGameCard", AddGameCard::create);

    auto* dataSource = new InstalledGameDS(m_gameManager.getInstalledGames(), [this](size_t index) {
        onGameCardClicked(index);
    });
    m_grid->setDataSource(dataSource);

    m_grid->setFocusChangeCallback([this](size_t index) {
        m_focusedIndex = static_cast<int>(index);
        ShellState::setIndexText(std::to_string(index + 1) + " / " + std::to_string(m_gameManager.getInstalledGames().size()));
    });
}

void AddGame::setNacpActionsAvailable(bool available) {
    setActionAvailable(brls::BUTTON_BACK, available);
    setActionAvailable(brls::BUTTON_Y, available);
}

void AddGame::startNacpLoader() {
    auto& games = m_gameManager.getInstalledGames();
    bool hasUnloaded = false;
    for (size_t i = 0; i < games.size(); i++) {
        if (!games[i].isLoaded) {
            hasUnloaded = true;
            break;
        }
    }

    if (!hasUnloaded) {
        setNacpActionsAvailable(true);
        return;
    }

    submitNextNacp();
}

void AddGame::submitNextNacp() {
    auto& games = m_gameManager.getInstalledGames();

    // 找离焦点最近的未加载游戏
    int bestIdx = -1;
    int bestDist = INT_MAX;
    for (size_t i = 0; i < games.size(); i++) {
        if (games[i].isLoaded) continue;
        int dist = std::abs(static_cast<int>(i) - m_focusedIndex);
        if (dist < bestDist) {
            bestDist = dist;
            bestIdx = static_cast<int>(i);
        }
    }

    if (bestIdx < 0) {
        setNacpActionsAvailable(true);
        return;
    }

    size_t gameIdx = static_cast<size_t>(bestIdx);
    auto token = m_stopSource.get_token();

    ThreadPool::instance().submit([this, gameIdx, &gameManager = m_gameManager](std::stop_token token) {
        auto meta = gameManager.fetchInstalledMetadata(gameIdx);
        if (token.stop_requested()) return;

        brls::sync([this, gameIdx, meta = std::move(meta), token]() {
            if (token.stop_requested()) return;
            applyMetadata(gameIdx, meta);
            submitNextNacp();
        });
    }, token);
}

void AddGame::applyMetadata(size_t gameIdx, const GameMetadata& meta) {
    auto& game = m_gameManager.getInstalledGames()[gameIdx];

    if (!meta.name.empty()) game.displayName = meta.name;
    if (!meta.version.empty()) game.version = meta.version;

    // 创建 NVG 纹理
    if (!meta.icon.empty()) {
        NVGcontext* vg = brls::Application::getNVGContext();
        int iconId = nvgCreateImageMem(vg, 0, const_cast<unsigned char*>(meta.icon.data()), meta.icon.size());
        if (iconId > 0) {
            game.iconId = iconId;
            auto& textureCache = brls::TextureCache::instance();
            std::string key = format::appIdHex(game.appId);
            if (textureCache.getCache(key) == 0) textureCache.addCache(key, iconId);
        }
    }

    game.isLoaded = true;

    // 刷新可见 Cell
    auto* cell = m_grid->getGridItemByIndex(gameIdx);
    if (cell) {
        auto* card = static_cast<AddGameCard*>(cell);
        card->setGame(game.displayName, game.version, game.modCount);
        if (game.iconId > 0) card->setIcon(game.iconId);
    }
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
