/**
 * StoreGameIconCache - 商店游戏图标本地缓存实现
 */

#include "core/storeGameIconCache.hpp"
#include "common/config.hpp"
#include "utils/fsHelper.hpp"
#include <mutex>

namespace {

    constexpr const char* KEY_ETAG = "etag";
    constexpr const char* KEY_LAST_MODIFIED = "lastModified";

    std::mutex g_iconFileMutex;

    bool replaceFile(const std::string& tempPath, const std::string& targetPath, const std::string& backupPath) {
        if (!fs::fileExists(targetPath)) return fs::moveFile(tempPath, targetPath);

        fs::deleteFile(backupPath);
        if (!fs::moveFile(targetPath, backupPath)) return false;

        if (fs::moveFile(tempPath, targetPath)) {
            fs::deleteFile(backupPath);
            return true;
        }

        fs::moveFile(backupPath, targetPath);
        return false;
    }

    std::string tempIconPath(const std::string& tid) {
        return StoreGameIconCache::iconPath(tid) + ".tmp";
    }

    std::string backupIconPath(const std::string& tid) {
        return StoreGameIconCache::iconPath(tid) + ".bak";
    }

} // namespace

bool StoreGameIconCache::load() {
    return m_json.load(config::storeGameIconCachePath);
}

bool StoreGameIconCache::save() {
    return m_json.save();
}

std::string StoreGameIconCache::iconPath(const std::string& tid) {
    return std::string(config::storeGameIconDir) + "/" + tid + ".webp";
}

std::vector<uint8_t> StoreGameIconCache::readIcon(const std::string& tid) {
    if (tid.empty()) return {};
    std::lock_guard lock(g_iconFileMutex);
    return fs::readFile(iconPath(tid));
}

bool StoreGameIconCache::writeIcon(const std::string& tid, const std::vector<uint8_t>& data) {
    if (tid.empty()) return false;
    if (data.empty()) return false;
    std::lock_guard lock(g_iconFileMutex);

    fs::ensureDir(config::storeGameIconDir);

    std::string path = iconPath(tid);
    std::string tempPath = tempIconPath(tid);
    std::string backupPath = backupIconPath(tid);

    fs::deleteFile(tempPath);
    fs::deleteFile(backupPath);

    if (fs::writeFile(tempPath, data.data(), data.size()) != 0) {
        fs::deleteFile(tempPath);
        return false;
    }

    if (!replaceFile(tempPath, path, backupPath)) {
        fs::deleteFile(tempPath);
        return false;
    }

    return true;
}

void StoreGameIconCache::deleteCache() {
    fs::removeDirAll(config::storeGameIconDir);
    fs::deleteFile(config::storeGameIconCachePath);
}

int64_t StoreGameIconCache::cacheSize(std::stop_token token) {
    int64_t total = 0;

    int64_t iconSize = fs::calcDirSize(config::storeGameIconDir, &token);
    if (iconSize > 0) total += iconSize;
    if (token.stop_requested()) return -1;

    int64_t metadataSize = fs::getFileSize(config::storeGameIconCachePath);
    if (metadataSize > 0) total += metadataSize;

    return total;
}

StoreGameIconCache::Metadata StoreGameIconCache::metadata(const std::string& tid) {
    Metadata result;
    if (tid.empty()) return result;

    result.etag = m_json.getString(tid, KEY_ETAG);
    result.lastModified = m_json.getString(tid, KEY_LAST_MODIFIED);
    return result;
}

void StoreGameIconCache::updateMetadata(const std::string& tid, const std::string& etag, const std::string& lastModified) {
    if (tid.empty()) return;

    m_json.setString(tid, KEY_ETAG, etag);
    m_json.setString(tid, KEY_LAST_MODIFIED, lastModified);
}
