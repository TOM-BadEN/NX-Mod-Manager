/**
 * StoreGameList - 商店游戏列表页面实现
 */

#include "activity/storeGameList.hpp"
#include <borealis/core/i18n.hpp>
#include "dataSource/storeGameListDS.hpp"
#include "view/storeGameCard.hpp"
#include "view/customDialog.hpp"
#include "activity/storeModList.hpp"
#include "utils/webpDecoder.hpp"
#include "core/audio.hpp"
#include "utils/pageNav.hpp"
#include "view/keyboardInput.hpp"
#include <switch.h>
#include <mutex>

StoreGameList::StoreGameList(GameManager& gameManager)
    : m_gameManager(gameManager) {}

StoreGameList::~StoreGameList() {
    m_alive->store(false);

    // 提前对所有异步任务发出停止信号，让 HTTP 请求并行中断
    m_loadTask.request_stop();
    m_iconLoader.request_stop();
    m_iconLoader2.request_stop();

    // 释放游戏图标纹理
    NVGcontext* vg = brls::Application::getNVGContext();
    for (auto& game : m_manager.storeGameList()) {
        if (game.iconId > 0) nvgDeleteImage(vg, game.iconId);
    }
}

void StoreGameList::onContentAvailable() {
    m_emptyHint->setVisibility(brls::Visibility::VISIBLE);
    m_manager.cacheInstalledTids();
    setupGrid();
    setupSearch();
    setupFilterMenu();
    loadNextPage();
}

void StoreGameList::setupGrid() {
    m_grid->setPadding(17, 15, 17, 40);
    m_grid->registerCell("StoreGameCard", StoreGameCard::create);
    m_grid->onNextPage([this] {
        if (!m_loading && m_manager.hasMore()) {
            loadNextPage();
        }
    });
    m_grid->setFocusChangeCallback([this](size_t index) {
        m_focusedIndex.store(static_cast<int>(index));
        m_frame->setIndexText(std::to_string(index + 1) + " / " + std::to_string(m_manager.total()));
        m_frame->setSubtitle(m_manager.storeGameList()[index].gameTid);
    });
}

void StoreGameList::loadNextPage() {
    m_loading = true;
    m_loadTask = util::async([this](std::stop_token token) {
        auto result = m_manager.fetchNextPage(token);
        if (token.stop_requested()) return;
        auto alive = m_alive;
        brls::sync([this, alive, result = std::move(result)]() mutable {
            if (!alive->load()) return;
            m_loading = false;
            if (!result.success) {
                auto goBack = [this] {
                    CustomDialog::close();
                    if (m_manager.storeGameList().empty()) {
                        auto alive = m_alive;
                        brls::sync([alive] {
                            if (!alive->load()) return;
                            brls::Application::popActivity();
                        });
                    }
                };
                CustomDialog::show(result.error, {{brls::getStr("storeGameList/ok"), goBack}}, goBack);
                return;
            }

            m_manager.appendPage(std::move(result));
            auto& list = m_manager.storeGameList();
            if (list.empty()) {
                m_emptyHint->setText(brls::getStr("storeGameList/noGames"));
                m_frame->setSubtitle("");
                brls::Application::giveFocus(m_emptyHint);
                return;
            }
            m_emptyHint->setVisibility(brls::Visibility::GONE);

            if (!m_grid->getDataSource()) {
                // 首次：设置数据源 + 获取焦点
                m_grid->setDataSource(new StoreGameListDS(m_manager.storeGameList(), [this](size_t index) {
                    auto& game = m_manager.storeGameList()[index];
                    pageNav::push(new StoreModList(game.gameTid, game.gameName, m_gameManager));
                }));
                brls::Application::giveFocus(m_grid);
            } else {
                // 后续分页：只通知数据变化，保持滚动位置
                m_grid->notifyDataChanged();
            }

            startIconLoader();
        });
    });
}

void StoreGameList::setupSearch() {
    m_frame->registerAction(brls::getStr("storeGameList/searchAction"), brls::BUTTON_BACK, [this](...) {
        Audio::instance()->play(SoundEffect::Enter);
        KeyboardInput::show([this](std::string result) {
            if (result == m_keyword) return;
            m_keyword = result;
            m_manager.setKeyword(m_keyword);
            m_frame->setActionAvailable(brls::BUTTON_Y, !m_keyword.empty());
            reloadData();
        }, brls::getStr("storeGameList/searchPlaceholder"), 50);
        return true;
    });

    m_frame->registerAction(brls::getStr("storeGameList/resetSearch"), brls::BUTTON_Y, [this](...) {
        Audio::instance()->play(SoundEffect::Click);
        m_keyword.clear();
        m_manager.setKeyword("");
        m_frame->setActionAvailable(brls::BUTTON_Y, false);
        reloadData();
        return true;
    });
    m_frame->setActionAvailable(brls::BUTTON_Y, false);
}

void StoreGameList::setupFilterMenu() {
    m_filterMenu.title = brls::getStr("storeGameList/filterTitle");
    m_filterMenu.shouldShowFakeHighlight = [this]{ return !m_manager.storeGameList().empty(); };

    auto addFilterItem = [this](const std::string& key, const std::string& descKey, GameFilterMode mode) {
        auto& item = m_filterMenu.addItem(brls::getStr(key), brls::getStr(descKey));
        item.setBadge([this, mode]{ return m_manager.getFilterMode() == mode ? std::string("\uE14B") : std::string(); });
        item.setAction([this, mode]{
            if (m_manager.getFilterMode() == mode) return;
            m_manager.setFilterMode(mode);
            reloadData();
        });
    };

    addFilterItem("storeGameList/filterAll", "storeGameList/filterAllDesc", GameFilterMode::All);
    addFilterItem("storeGameList/filterInstalled", "storeGameList/filterInstalledDesc", GameFilterMode::Installed);
    addFilterItem("storeGameList/filterNotInstalled", "storeGameList/filterNotInstalledDesc", GameFilterMode::NotInstalled);

    m_frame->registerAction(brls::getStr("storeGameList/filterAction"), brls::BUTTON_X, [this](...) {
        Audio::instance()->play(SoundEffect::Enter);
        m_filterMenu.show();
        return true;
    });
}

void StoreGameList::reloadData() {
    // 让旧异步回调失效（不阻塞）
    m_alive->store(false);
    m_alive = std::make_shared<std::atomic<bool>>(true);

    // 清空纹理
    NVGcontext* vg = brls::Application::getNVGContext();
    for (auto& game : m_manager.storeGameList()) {
        if (game.iconId > 0) nvgDeleteImage(vg, game.iconId);
    }

    // 重置数据
    m_manager.reset();
    m_grid->setDefaultCellFocus(0);
    m_grid->setDataSource(nullptr);
    m_emptyHint->setText(brls::getStr("storeGameList/loadingHint"));
    m_emptyHint->setVisibility(brls::Visibility::VISIBLE);
    m_loading = false;

    // 焦点交给加载提示，避免悬空（旧 cell 已销毁）
    brls::Application::giveFocus(m_emptyHint);

    // 更新标题
    m_frame->setTitle(m_keyword.empty() ? brls::getStr("storeGameList/titleDefault") : brls::getStr("storeGameList/titleSearch", m_keyword));
    m_frame->setSubtitle("");

    loadNextPage();
}

void StoreGameList::startIconLoader() {
    // 主线程：构建快照（只包含未加载图标的游戏）
    std::vector<std::pair<int, std::string>> tasks;
    auto& list = m_manager.storeGameList();
    for (int i = 0; i < (int)list.size(); i++) {
        if (list[i].iconId == 0) tasks.push_back({i, list[i].gameTid});
    }
    if (tasks.empty()) return;

    // 共享任务队列 + 互斥锁（两个线程从同一个队列取任务）
    auto remaining = std::make_shared<std::vector<std::pair<int, std::string>>>(std::move(tasks));
    auto mutex = std::make_shared<std::mutex>();

    // 工作函数：循环取离焦点最近的任务 → 下载 → 主线程解码
    auto worker = [this, remaining, mutex](std::stop_token token) {
        while (!token.stop_requested()) {
            // 取离焦点最近的任务
            std::pair<int, std::string> task;
            {
                std::lock_guard lock(*mutex);
                if (remaining->empty()) break;
                int center = m_focusedIndex.load();
                int bestIdx = 0;
                int bestDist = INT_MAX;
                for (int i = 0; i < (int)remaining->size(); i++) {
                    int dist = std::abs((*remaining)[i].first - center);
                    if (dist < bestDist) {
                        bestDist = dist;
                        bestIdx = i;
                    }
                }
                task = (*remaining)[bestIdx];
                remaining->erase(remaining->begin() + bestIdx);
            }

            auto [gameIdx, tid] = task;

            // 后台下载 WebP 图标
            auto response = m_manager.fetchIcon(tid, token);
            if (token.stop_requested()) break;
            if (!response.ok() || response.body.empty()) {
                auto alive = m_alive;
                brls::sync([this, alive, gameIdx] {
                    if (!alive->load()) return;
                    m_manager.storeGameList()[gameIdx].iconId = -1;
                });
                continue;
            }

            // 主线程：解码 WebP → 创建纹理 → 更新卡片
            auto body = std::move(response.body);
            auto alive = m_alive;
            brls::sync([this, alive, gameIdx, body = std::move(body)] {
                if (!alive->load()) return;
                if (m_manager.storeGameList()[gameIdx].iconId != 0) return;
                auto img = webpDecoder::decode(body.data(), body.size());
                int textureId = img.width > 0 ? nvgCreateImageRGBA(brls::Application::getNVGContext(), img.width, img.height, 0, img.pixels.data()) : 0;
                m_manager.storeGameList()[gameIdx].iconId = (textureId > 0) ? textureId : -1;
                if (textureId > 0) {
                    auto* cell = m_grid->getGridItemByIndex(gameIdx);
                    if (cell) static_cast<StoreGameCard*>(cell)->setIcon(textureId);
                }
            });

            svcSleepThread(1000000ULL);  // 1ms
        }
    };

    // 两个异步任务并行下载
    m_iconLoader = util::async(worker);
    m_iconLoader2 = util::async(worker);
}
