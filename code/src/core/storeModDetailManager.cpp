/**
 * StoreModDetailManager - 商店模组详情数据管理实现
 */

#include "core/storeModDetailManager.hpp"
#include "common/config.hpp"
#include "utils/fsHelper.hpp"
#include "utils/jsonFile.hpp"
#include "utils/format.hpp"

StoreModDetailManager::StoreModDetailManager(int modId, std::string gameTid)
    : m_modId(modId), m_gameTid(std::move(gameTid)) {}

// 模组详情

void StoreModDetailManager::setDetail(api::mod::ModDetail detail) {
    m_detail = std::move(detail);
}

api::mod::ModDetail& StoreModDetailManager::getDetail() {
    return m_detail;
}

// 截图

StoreModDetailManager::Screenshots& StoreModDetailManager::getScreenshots() {
    return m_screenshots;
}

// 点赞/点踩

bool StoreModDetailManager::applyLikeToggle() {
    m_detail.deviceLiked = !m_detail.deviceLiked;
    m_detail.likeCount += m_detail.deviceLiked ? 1 : -1;
    return m_detail.deviceLiked;
}

bool StoreModDetailManager::applyDislikeToggle() {
    m_detail.deviceDisliked = !m_detail.deviceDisliked;
    m_detail.dislikeCount += m_detail.deviceDisliked ? 1 : -1;
    return m_detail.deviceDisliked;
}

// 商店下载/更新：根据详情构造本地 ModInfo

ModInfo StoreModDetailManager::buildDownloadedModInfo(std::string modDirName, std::string modDir) const {
    ModInfo info;
    info.displayName = m_detail.modName;
    info.type        = m_detail.modType;
    info.description = m_detail.description;
    info.modVersion  = m_detail.modVersion;
    info.gameVersion = m_detail.gameVersion;
    info.author      = m_detail.author;
    info.authorLink  = m_detail.authorLink;
    info.size        = format::fileSize(m_detail.fileSize);
    info.modID       = m_detail.modId;
    info.fileCrc32   = m_detail.fileCrc32;
    info.isInstalled = false;
    info.hasUpdate   = false;
    info.isZip       = true;
    info.dirName     = std::move(modDirName);
    info.path        = std::move(modDir);
    return info;
}

ModInfo StoreModDetailManager::createDownloadedMod(const std::string& gameDir,
    std::string modDirName, const std::string& tempZipPath) {
    std::string modDir = fs::ensureUniqueDirPath(gameDir + "/" + modDirName);
    modDirName = modDir.substr(modDir.rfind('/') + 1);
    fs::ensureDir(modDir);

    fs::moveFile(tempZipPath, modDir + "/" + modDirName + ".zip");

    m_detail.downloaded = true;

    return buildDownloadedModInfo(std::move(modDirName), std::move(modDir));
}

// 商店下载安装

ModInfo StoreModDetailManager::saveDownloadedMod(const std::string& gameDir, std::string modDirName, const std::string& tempZipPath) {
    ModInfo info = createDownloadedMod(gameDir, modDirName, tempZipPath);

    JsonFile modJson;
    modJson.load(gameDir + config::modInfoFile);
    modJson.setString(info.dirName, "displayName", info.displayName);
    modJson.setString(info.dirName, "type", info.type);
    modJson.setString(info.dirName, "description", info.description);
    modJson.setString(info.dirName, "modVersion", info.modVersion);
    modJson.setString(info.dirName, "gameVersion", info.gameVersion);
    modJson.setString(info.dirName, "author", info.author);
    modJson.setString(info.dirName, "authorLink", info.authorLink);
    modJson.setString(info.dirName, "size", info.size);
    modJson.setString(info.dirName, "modID", std::to_string(info.modID));
    modJson.setString(info.dirName, "fileCrc32", info.fileCrc32);
    modJson.save();

    return info;
}

// 留言分页

void StoreModDetailManager::appendCommentPage(api::comment::CommentListResult result) {
    m_commentPage++;
    m_commentTotal = result.total;
    for (auto& comment : result.list) {
        m_comments.push_back(std::move(comment));
    }
}

std::vector<api::comment::Comment>& StoreModDetailManager::getComments() {
    return m_comments;
}

bool StoreModDetailManager::hasMoreComments() const {
    return (int)m_comments.size() < m_commentTotal;
}
