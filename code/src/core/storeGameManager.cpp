/**
 * StoreGameManager - 商店游戏列表数据管理实现
 */

#include "core/storeGameManager.hpp"
#include "utils/gameNacp.hpp"
#include "utils/format.hpp"

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

void StoreGameManager::cacheInstalledTids() {
    auto tids = gameNacp::getInstalledGameTids();
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

int StoreGameManager::currentPage() const {
    return m_currentPage;
}

const std::string& StoreGameManager::tidsJson() const {
    return m_tidsJson;
}

void StoreGameManager::reset() {
    m_storeGameList.clear();
    m_currentPage = 0;
    m_total = 0;
}

std::string StoreGameManager::getLocalVersion(size_t index) {
    auto& game = m_storeGameList[index];
    if (!game.installed) return "...";
    return gameNacp::getVersion(format::appIdFromHex(game.gameTid));
}
