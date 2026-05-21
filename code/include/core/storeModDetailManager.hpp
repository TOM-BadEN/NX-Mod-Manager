/**
 * StoreModDetailManager - 商店模组详情数据管理
 * 持有模组详情、截图纹理和留言列表，纯数据层，不涉及 UI
 */

#pragma once

#include <vector>
#include <string>
#include <stop_token>
#include "api/mod.hpp"
#include "api/comment.hpp"
#include "api/url.hpp"
#include "utils/http.hpp"
#include "common/modInfo.hpp"

class StoreModDetailManager {

public:
    // 截图纹理（0 = 未加载或加载失败）
    struct Screenshots {
        int texture1 = 0;
        int texture2 = 0;
    };

    /**
     * @brief 构造商店模组详情管理器
     * @param modId 模组 ID
     * @param gameTid 游戏 TID
     * @param downloaded 是否已下载
     */
    StoreModDetailManager(int modId, std::string gameTid, bool downloaded = false);

    /**
     * @brief 获取模组详情（子线程调用）
     * @param token 取消令牌
     */
    api::mod::ModDetailResult fetchDetail(std::stop_token token = {});

    /**
     * @brief 设置模组详情（主线程调用）
     * @param detail 模组详情数据
     */
    void setDetail(api::mod::ModDetail detail);

    /** @brief 获取模组详情引用 */
    api::mod::ModDetail& getDetail();

    /**
     * @brief 下载截图（子线程调用，返回原始图片数据）
     * @param index 截图索引
     * @param token 取消令牌
     */
    http::Response fetchScreenshot(int index, std::stop_token token);

    /** @brief 获取截图纹理引用（Activity 解码后填入） */
    Screenshots& getScreenshots();

    /**
     * @brief 点赞（子线程调用 API）
     * @param token 取消令牌
     */
    api::ApiResult toggleLike(std::stop_token token = {});

    /**
     * @brief 点踩（子线程调用 API）
     * @param token 取消令牌
     */
    api::ApiResult toggleDislike(std::stop_token token = {});

    /** @brief 翻转 deviceLiked + 更新 likeCount，返回新状态 */
    bool applyLikeToggle();

    /** @brief 翻转 deviceDisliked + 更新 dislikeCount，返回新状态 */
    bool applyDislikeToggle();

    /**
     * @brief 商店下载：创建 mod 目录、移动 zip，不写 modInfo.json
     * @param gameDir 游戏目录路径
     * @param modDirName mod 目录名
     * @param tempZipPath 临时 zip 文件路径
     * @return 构造好的 ModInfo
     */
    ModInfo createDownloadedMod(const std::string& gameDir, std::string modDirName, const std::string& tempZipPath);

    /**
     * @brief 商店下载：创建 mod 目录、移动 zip、写 modInfo.json
     * @param gameDir 游戏目录路径
     * @param modDirName mod 目录名
     * @param tempZipPath 临时 zip 文件路径
     * @return 构造好的 ModInfo
     */
    ModInfo saveDownloadedMod(const std::string& gameDir, std::string modDirName, const std::string& tempZipPath);

    /**
     * @brief 提交留言（子线程调用）
     * @param nickname 昵称
     * @param content 留言内容
     * @param token 取消令牌
     */
    api::comment::CommentSubmitResult submitComment(const std::string& nickname, const std::string& content, std::stop_token token = {});

    /**
     * @brief 获取下一页留言（子线程调用）
     * @param token 取消令牌
     */
    api::comment::CommentListResult fetchNextCommentPage(std::stop_token token = {});

    /**
     * @brief 追加一页留言（主线程调用）
     * @param result 留言数据结果
     */
    void appendCommentPage(api::comment::CommentListResult result);

    /** @brief 获取留言列表引用 */
    std::vector<api::comment::Comment>& getComments();

    /** @brief 是否还有更多留言 */
    bool hasMoreComments() const;

    /** @brief 获取模组 ID */
    int modId() const { return m_modId; }

    /** @brief 获取游戏 TID */
    const std::string& gameTid() const { return m_gameTid; }

    /** @brief 获取当前留言页码 */
    int commentPage() const { return m_commentPage; }

private:
    int m_modId;                                       // 模组 ID
    std::string m_gameTid;                             // 游戏 TID
    bool m_downloaded = false;                         // 是否已下载

    api::mod::ModDetail m_detail;                      // 模组详情
    Screenshots m_screenshots;                         // 截图纹理

    std::vector<api::comment::Comment> m_comments;     // 留言列表
    int m_commentPage = 0;                             // 当前留言页码
    int m_commentTotal = 0;                            // 留言总数
};
