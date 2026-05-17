/**
 * 添加游戏页面实现文件
 */

#include "activity/addGameActivity.hpp"
#include <borealis/core/i18n.hpp>
#include "core/modManager.hpp"
#include "core/audio.hpp"
#include "activity/searchActivity.hpp"
#include "view/addGameCard.hpp"
#include "dataSource/installedGameDS.hpp"
#include "utils/format.hpp"
#include "view/customDialog.hpp"
#include "utils/pageNav.hpp"
#include "view/mtpDialog.hpp"
#include "view/ftpDialog.hpp"
#include "view/keyboardInput.hpp"
#include <borealis/core/cache_helper.hpp>
#include "view/longPressDialog.hpp"
#include "common/settings.hpp"

AddGameActivity::AddGameActivity(GameManager& gameManager)
 : m_gameManager(gameManager) {

}

AddGameActivity::~AddGameActivity() {
    m_stopSource.request_stop();
}

void AddGameActivity::onContentAvailable() {
    m_gameManager.sortInstalledGames(true);

    // B 键：退出（Home::onResume 会自动检查 pendingFocus 并刷新）
    m_frame->registerAction(brls::getStr("addGameActivity/back"), brls::BUTTON_B, [this](brls::View*) {
        Audio::instance()->play(SoundEffect::Enter);
        pageNav::pop(pageNav::Anim::SLIDE_LEFT);
        return true;
    });

    // ZL：返回主页（隐藏 hint）
    m_frame->registerAction("", brls::BUTTON_LT, [this](...) {
        Audio::instance()->play(SoundEffect::Enter);
        pageNav::pop(pageNav::Anim::SLIDE_LEFT);
        return true;
    }, true);

    // - 键：打开搜索页面
    m_frame->registerAction(brls::getStr("addGameActivity/search"), brls::BUTTON_BACK, [this](...) {
        Audio::instance()->play(SoundEffect::Enter);
        std::vector<std::string> names;
        for (auto& game : m_gameManager.getInstalledGames()) {
            names.push_back(game.displayName);
        }
        pageNav::push(new SearchActivity(names, [this](int index) {
            m_grid->selectRowAt(index, false);
            m_grid->instantFocus(index);
        }));
        return true;
    });

    // Y 键：切换排序方向（NACP 加载完成前禁用）
    m_frame->registerAction(brls::getStr("addGameActivity/sortAsc"), brls::BUTTON_Y, [this](...) {
        Audio::instance()->play(SoundEffect::Click);
        toggleSort();
        return true;
    });
    m_frame->setActionAvailable(brls::BUTTON_Y, false);

    // X 键：打开添加模组菜单
    setupMenu();
    m_frame->registerAction(brls::getStr("addGameActivity/menu"), brls::BUTTON_X, [this](...) {
        Audio::instance()->play(SoundEffect::Enter);
        m_addModMenu.show();
        return true;
    });

    loadVirtualGameIcon();
    setupGridPage();
    startNacpLoader();
    runFirstLaunchDialog();
}

void AddGameActivity::loadVirtualGameIcon() {
    auto* img = new brls::Image();
    img->setImageFromRes("img/game/addGame.png");
    int iconId = img->getTexture();
    delete img;

    if (iconId > 0) m_gameManager.getInstalledGames()[0].iconId = iconId;
}

void AddGameActivity::setupMenu() {
    m_addModMenu.title = brls::getStr("addGameActivity/menuTitle");

    auto& mtpItem = m_addModMenu.addItem(brls::getStr("addGameActivity/menuMtp"), brls::getStr("addGameActivity/menuMtpDesc"));
    mtpItem.setAction([] {
        std::vector<MtpMount> mounts = {
            {"/mods2/!temp_mods", "Add Mod"},
        };
        mtpDialog::open(mounts);
    });

    auto& ftpItem = m_addModMenu.addItem(brls::getStr("addGameActivity/menuFtp"), brls::getStr("addGameActivity/menuFtpDesc"));
    ftpItem.setAction([] {
        std::vector<FtpMount> mounts = {
            {"/mods2/!temp_mods", "Add Mod"},
        };
        ftpDialog::open(mounts);
    });
}

void AddGameActivity::onGameCardClicked(size_t index) {
    auto mods = ModManager::scanTransitMods();
    if (mods.empty()) {
        CustomDialog::show(brls::getStr("addGameActivity/transitEmpty"), {{brls::getStr("addGameActivity/ok"), [] { CustomDialog::close(); }}});
        return;
    }

    if (index == 0) onVirtualGameCardClicked(std::move(mods));
    else onRealGameCardClicked(index, std::move(mods));
}

void AddGameActivity::onRealGameCardClicked(size_t index, std::vector<fs::DirEntry> mods) {
    auto* menu = new MenuPageConfig();
    menu->title = brls::getStr("addGameActivity/selectMod");
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
            CustomDialog::show(brls::getStr("addGameActivity/addSuccess", added), {
                {brls::getStr("addGameActivity/continueAdd"), [] { CustomDialog::close(); }},
                {brls::getStr("addGameActivity/backToHome"), [] { CustomDialog::close([] { pageNav::pop(pageNav::Anim::SLIDE_LEFT); });}},
            });
        };

        CustomDialog::show(brls::getStr("addGameActivity/addConfirm", selected.size()), {
            {brls::getStr("addGameActivity/cancel"), [] { CustomDialog::close(); }},
            {brls::getStr("addGameActivity/confirm"), onConfirmAdd}
        });
    };
    menu->show();
}

void AddGameActivity::onVirtualGameCardClicked(std::vector<fs::DirEntry> mods) {
    auto onTidConfirm = [this, mods = std::move(mods)](std::string tid) mutable {
        uint64_t appId = format::appIdFromHex(tid);
        if (!format::appIdIsValid(appId)) {
            CustomDialog::show(brls::getStr("addGameActivity/tidInvalid"), {{brls::getStr("addGameActivity/ok"), [] { CustomDialog::close(); }}});
            return;
        }

        int installedIdx = m_gameManager.findInstalledByAppId(appId);
        if (installedIdx >= 0) {
            onRealGameCardClicked(static_cast<size_t>(installedIdx), std::move(mods));
            return;
        }

        // 虚拟游戏：Switch 上未安装，照抄模组选择流程
        auto* menu = new MenuPageConfig();
        menu->title = brls::getStr("addGameActivity/selectMod");
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
                CustomDialog::show(brls::getStr("addGameActivity/addSuccess", added), {
                    {brls::getStr("addGameActivity/continueAdd"), [] { CustomDialog::close(); }},
                    {brls::getStr("addGameActivity/backToHome"), [] { CustomDialog::close([] { pageNav::pop(pageNav::Anim::SLIDE_LEFT); });}},
                });
            };
            CustomDialog::show(brls::getStr("addGameActivity/addConfirm", selected.size()), {
                {brls::getStr("addGameActivity/cancel"), [] { CustomDialog::close(); }},
                {brls::getStr("addGameActivity/confirm"), onConfirmAdd}
            });
        };
        menu->show();
    };
    KeyboardInput::show(onTidConfirm, brls::getStr("addGameActivity/tidPlaceholder"), 16);
}

void AddGameActivity::setupGridPage() {
    m_grid->setPadding(17, 15, 17, 40);
    m_grid->registerCell("AddGameCard", AddGameCard::create);

    auto* dataSource = new InstalledGameDS(m_gameManager.getInstalledGames(), [this](size_t index) {
        onGameCardClicked(index);
    });
    
    m_grid->setDataSource(dataSource);

    m_grid->setFocusChangeCallback([this](size_t index) {
        m_focusedIndex = static_cast<int>(index);
        m_frame->setIndexText(std::to_string(index + 1) + " / " + std::to_string(m_gameManager.getInstalledGames().size()));
    });
}

void AddGameActivity::startNacpLoader() {
    auto& games = m_gameManager.getInstalledGames();
    bool hasUnloaded = false;
    for (size_t i = 0; i < games.size(); i++) {
        if (!games[i].isLoaded) { 
            hasUnloaded = true; 
            break; 
        }
    }

    if (!hasUnloaded) {
        m_frame->setActionAvailable(brls::BUTTON_Y, true);
        return;
    }

    submitNextNacp();
}

void AddGameActivity::submitNextNacp() {
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
        m_frame->setActionAvailable(brls::BUTTON_Y, true);
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

void AddGameActivity::applyMetadata(size_t gameIdx, const GameMetadata& meta) {
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

void AddGameActivity::runFirstLaunchDialog() {
    if (!Settings::getBool("addGame", "firstLaunch", true)) return;
    brls::sync([this]() {
        auto onConfirm = [this]() {
            Settings::setBool("addGame", "firstLaunch", false);
            LongPressDialog::close();
        };
        LongPressDialog::show(brls::getStr("addGameActivity/firstLaunchNotice"), brls::getStr("addGameActivity/firstLaunchBtn"), 5.0f, onConfirm);
    });
}

void AddGameActivity::toggleSort() {
    m_sortAsc = !m_sortAsc;
    m_gameManager.sortInstalledGames(m_sortAsc);
    m_grid->setDefaultCellFocus(0);
    m_grid->reloadData();
    m_grid->instantFocus(0);
    m_frame->updateActionHint(brls::BUTTON_Y, m_sortAsc ? brls::getStr("addGameActivity/sortAsc") : brls::getStr("addGameActivity/sortDesc"));
    brls::Application::getGlobalHintsUpdateEvent()->fire();
}
