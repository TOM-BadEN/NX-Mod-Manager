/**
 * 添加游戏页面实现文件
 */

#include "activity/addGameActivity.hpp"
#include <borealis/core/i18n.hpp>
#include "core/modManager.hpp"
#include "core/audio.hpp"
#include "activity/searchActivity.hpp"
#include "view/gameCard.hpp"
#include "dataSource/installedGameDS.hpp"
#include "utils/format.hpp"
#include "view/customDialog.hpp"
#include "utils/pageNav.hpp"
#include "view/mtpDialog.hpp"
#include "view/ftpDialog.hpp"
#include "view/keyboardInput.hpp"
#include <borealis/core/cache_helper.hpp>
#include <switch.h>

AddGameActivity::AddGameActivity(GameManager& gameManager)
 : m_gameManager(gameManager) {

}

AddGameActivity::~AddGameActivity() {
    m_alive->store(false);
}

void AddGameActivity::onContentAvailable() {
    auto& installedGames = m_gameManager.getInstalledGames();

    m_gameManager.sortInstalledGames(true);

    // B 键：退出（Home::onResume 会自动检查 pendingFocus 并刷新）
    m_frame->registerAction(brls::getStr("addGameActivity/back"), brls::BUTTON_B, [this](brls::View*) {
        Audio::instance()->play(SoundEffect::Enter);
        brls::Application::popActivity(brls::TransitionAnimation::NONE);
        return true;
    });

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
            if (cell) static_cast<GameCard*>(cell)->setGame(installed.displayName, installed.version, installed.modCount);
            CustomDialog::show(brls::getStr("addGameActivity/addSuccess", added), {{brls::getStr("addGameActivity/ok"), [] { CustomDialog::close(); }}});
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
                CustomDialog::show(brls::getStr("addGameActivity/addSuccess", added), {{brls::getStr("addGameActivity/ok"), [] { CustomDialog::close(); }}});
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
    m_grid->registerCell("GameCard", GameCard::create);

    auto* dataSource = new InstalledGameDS(m_gameManager.getInstalledGames(), [this](size_t index) {
        onGameCardClicked(index);
    });
    
    m_grid->setDataSource(dataSource);

    m_grid->setFocusChangeCallback([this](size_t index) {
        m_focusedIndex.store(static_cast<int>(index));
        m_frame->setIndexText(std::to_string(index + 1) + " / " + std::to_string(m_gameManager.getInstalledGames().size()));
    });
}

void AddGameActivity::startNacpLoader() {
    // 构建未加载游戏的任务列表
    auto& games = m_gameManager.getInstalledGames();
    std::vector<size_t> tasks;
    for (size_t i = 0; i < games.size(); i++) {
        if (!games[i].isLoaded) tasks.push_back(i);
    }

    if (tasks.empty()) {
        m_frame->setActionAvailable(brls::BUTTON_Y, true);
        return;
    }

    m_nacpLoader = util::async([this, tasks](std::stop_token token) mutable {
        m_gameManager.openNacp();

        while (!tasks.empty() && !token.stop_requested()) {
            // 找离当前焦点最近的任务
            int center = m_focusedIndex.load();
            size_t bestIdx = 0;
            int bestDist = INT_MAX;
            for (size_t i = 0; i < tasks.size(); i++) {
                int dist = std::abs(static_cast<int>(tasks[i]) - center);
                if (dist < bestDist) {
                    bestDist = dist;
                    bestIdx = i;
                }
            }

            size_t gameIdx = tasks[bestIdx];
            tasks.erase(tasks.begin() + bestIdx);

            // 后台线程获取 NACP 数据
            auto meta = m_gameManager.fetchInstalledMetadata(gameIdx);
            if (meta.name.empty() && meta.version.empty() && meta.icon.empty()) continue;

            // 回主线程更新数据和 UI
            auto alive = m_alive;
            brls::sync([this, alive, gameIdx, meta = std::move(meta)]() {
                if (!alive->load()) return;
                applyMetadata(gameIdx, meta);
            });

            svcSleepThread(1000000ULL);  // 1ms
        }

        m_gameManager.closeNacp();

        if (!token.stop_requested()) {
            auto alive = m_alive;
            brls::sync([this, alive]() {
                if (!alive->load()) return;
                m_frame->setActionAvailable(brls::BUTTON_Y, true);
            });
        }
    });
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
        auto* card = static_cast<GameCard*>(cell);
        card->setGame(game.displayName, game.version, game.modCount);
        card->setNameColor(brls::Application::getTheme()["app/textHighlight"]);
        if (game.iconId > 0) card->setIcon(game.iconId);
    }
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
