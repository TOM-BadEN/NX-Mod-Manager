/**
 * StoreModDetailManager - 商店模组详情数据管理实现
 */

#include "core/storeModDetailManager.hpp"
#include "common/config.hpp"
#include "utils/fsHelper.hpp"
#include "utils/jsonFile.hpp"
#include "utils/format.hpp"

static constexpr int COMMENT_PAGE_SIZE = 20;

StoreModDetailManager::StoreModDetailManager(int modId, std::string gameTid)
    : m_modId(modId), m_gameTid(std::move(gameTid)) {
    m_record.load();
}

// 模组详情

api::mod::ModDetailResult StoreModDetailManager::fetchDetail(std::stop_token token) {
    return api::mod::fetchModDetail(m_modId, token);
}

void StoreModDetailManager::setDetail(api::mod::ModDetail detail) {
    detail.isInstalled = m_record.has(detail.modId);
    m_detail = std::move(detail);
}

api::mod::ModDetail& StoreModDetailManager::getDetail() {
    return m_detail;
}

// 截图

http::Response StoreModDetailManager::fetchScreenshot(int index, std::stop_token token) {
    return http::get(api::url::mod::screenshot(m_gameTid, m_modId, index), token);
}

StoreModDetailManager::Screenshots& StoreModDetailManager::getScreenshots() {
    return m_screenshots;
}

// 点赞/点踩

api::ApiResult StoreModDetailManager::toggleLike(std::stop_token token) {
    return api::mod::toggleLike(m_modId, token);
}

api::ApiResult StoreModDetailManager::toggleDislike(std::stop_token token) {
    return api::mod::toggleDislike(m_modId, token);
}

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

// 商店下载安装

ModInfo StoreModDetailManager::installMod(const std::string& gameDir, std::string modDirName, const std::string& tempZipPath) {
    std::string modDir = fs::ensureUniqueDirPath(gameDir + "/" + modDirName);
    modDirName = modDir.substr(modDir.rfind('/') + 1);
    fs::ensureDir(modDir);

    fs::moveFile(tempZipPath, modDir + "/" + modDirName + ".zip");

    JsonFile modJson;
    modJson.load(gameDir + config::modInfoFile);
    modJson.setString(modDirName, "displayName", m_detail.modName);
    modJson.setString(modDirName, "type", m_detail.modType);
    modJson.setString(modDirName, "description", m_detail.description);
    modJson.setString(modDirName, "modVersion", m_detail.modVersion);
    modJson.setString(modDirName, "gameVersion", m_detail.gameVersion);
    modJson.setString(modDirName, "author", m_detail.author);
    modJson.setString(modDirName, "authorLink", m_detail.authorLink);
    modJson.setString(modDirName, "size", format::fileSize(m_detail.fileSize));
    modJson.setString(modDirName, "modID", std::to_string(m_detail.modId));
    modJson.save();

    m_record.add(m_modId);
    m_record.save();

    m_detail.isInstalled = true;

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
    info.isZip       = true;
    info.dirName     = modDirName;
    info.path        = modDir;
    return info;
}

// 提交留言

api::comment::CommentSubmitResult StoreModDetailManager::submitComment(
    const std::string& nickname, const std::string& content, std::stop_token token) {
    return api::comment::submitComment(m_modId, nickname, content, token);
}

// 留言分页

api::comment::CommentListResult StoreModDetailManager::fetchNextCommentPage(std::stop_token token) {
    return api::comment::fetchComments(m_modId, m_commentPage + 1, COMMENT_PAGE_SIZE, token);
}

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
