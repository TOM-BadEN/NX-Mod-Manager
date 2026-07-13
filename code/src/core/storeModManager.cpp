/**
 * StoreModManager - 商店模组列表数据管理实现
 */

#include "core/storeModManager.hpp"

StoreModManager::StoreModManager(std::string gameTid)
    : m_gameTid(std::move(gameTid)) {}

void StoreModManager::appendPage(api::mod::ModListResult result) {
    m_currentPage++;
    m_total = result.total;
    for (auto& mod : result.list) {
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

