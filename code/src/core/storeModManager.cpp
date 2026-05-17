/**
 * StoreModManager - 商店模组列表数据管理实现
 */

#include "core/storeModManager.hpp"

static constexpr int PAGE_SIZE = 20;

StoreModManager::StoreModManager(std::string gameTid)
    : m_gameTid(std::move(gameTid)) {
    m_record.load();
}

api::mod::ModListResult StoreModManager::fetchNextPage(std::stop_token token) {
    return api::mod::fetchModList(m_gameTid, m_currentPage + 1, PAGE_SIZE, m_sort, m_keyword, m_version, m_modType, token);
}

void StoreModManager::appendPage(api::mod::ModListResult result) {
    m_currentPage++;
    m_total = result.total;
    for (auto& mod : result.list) {
        mod.isInstalled = m_record.has(mod.modId);
        m_storeModList.push_back(std::move(mod));
    }
}

void StoreModManager::reset() {
    m_storeModList.clear();
    m_currentPage = 0;
    m_total = 0;
}

std::vector<api::mod::ModList>& StoreModManager::storeModList() {
    return m_storeModList;
}

int StoreModManager::total() const {
    return m_total;
}

bool StoreModManager::hasMore() const {
    return (int)m_storeModList.size() < m_total;
}

const std::string& StoreModManager::gameTid() const {
    return m_gameTid;
}

api::mod::ModGameVersionsResult StoreModManager::fetchGameVersions(std::stop_token token) {
    return api::mod::fetchModGameVersions(m_gameTid, token);
}

void StoreModManager::resetFilter() {
    m_sort = "latest";
    m_version.clear();
    m_modType.clear();
}

void StoreModManager::setSort(const std::string& sort) {
    m_sort = sort;
}

void StoreModManager::setKeyword(const std::string& keyword) {
    m_keyword = keyword;
}

void StoreModManager::setVersion(const std::string& version) {
    m_version = version;
}

void StoreModManager::setModType(const std::string& modType) {
    m_modType = modType;
}

