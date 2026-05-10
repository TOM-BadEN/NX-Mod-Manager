/**
 * StoreGameManager - 商店游戏列表数据管理实现
 */

#include "core/storeGameManager.hpp"
#include "core/gameNACP.hpp"
#include "utils/format.hpp"

static constexpr int PAGE_SIZE = 20;

api::game::GameListResult StoreGameManager::fetchNextPage(std::stop_token token) {
    if (m_filterMode == GameFilterMode::Installed) return api::game::fetchInstalledGameList(m_currentPage + 1, PAGE_SIZE, m_keyword, m_tidsJson, true, token);
    else if (m_filterMode == GameFilterMode::NotInstalled) return api::game::fetchInstalledGameList(m_currentPage + 1, PAGE_SIZE, m_keyword, m_tidsJson, false, token);
    else return api::game::fetchGameList(m_currentPage + 1, PAGE_SIZE, m_keyword, token);
}

http::Response StoreGameManager::fetchIcon(const std::string& gameTid, std::stop_token token) {
    return http::downloadImage(api::url::game::icon(gameTid), token);
}

void StoreGameManager::appendPage(api::game::GameListResult result) {
    m_currentPage++;
    m_total = result.total;
    for (auto& game : result.list) {
        uint64_t tid = format::appIdFromHex(game.gameTid);
        game.installed = m_installedTids.count(tid) > 0;
        m_storeGameList.push_back(std::move(game));
    }
}

std::vector<api::game::GameList>& StoreGameManager::storeGameList() {
    return m_storeGameList;
}

int StoreGameManager::total() const {
    return m_total;
}

bool StoreGameManager::hasMore() const {
    return (int)m_storeGameList.size() < m_total;
}

void StoreGameManager::setKeyword(const std::string& keyword) {
    m_keyword = keyword;
}

void StoreGameManager::cacheInstalledTids() {
    auto tids = GameNACP::getInstalledGameTids();
    m_installedTids = {tids.begin(), tids.end()};
    m_tidsJson = "[";
    for (size_t i = 0; i < tids.size(); i++) {
        if (i > 0) m_tidsJson += ',';
        m_tidsJson += '"' + format::appIdHex(tids[i]) + '"';
    }
    m_tidsJson += ']';
}

void StoreGameManager::setFilterMode(GameFilterMode mode) {
    m_filterMode = mode;
}

GameFilterMode StoreGameManager::getFilterMode() const {
    return m_filterMode;
}

void StoreGameManager::reset() {
    m_storeGameList.clear();
    m_currentPage = 0;
    m_total = 0;
}
