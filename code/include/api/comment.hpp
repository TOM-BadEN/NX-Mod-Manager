/**
 * api/comment - 留言相关 API 请求
 * 封装 HTTP 请求 + JSON 解析，返回干净的业务数据
 */

#pragma once

#include "api/apiResult.hpp"
#include <string>
#include <vector>
#include <stop_token>

namespace api::comment {

// 单条留言数据
struct Comment {
    int commentId = 0;              // 留言 ID
    std::string nickname;           // 昵称
    std::string content;            // 留言内容
    std::string createTime;         // 创建时间（格式：YYYY-MM-DD HH:MM:SS）
};

// 获取留言列表的结果
struct CommentListResult : api::ApiResult {
    int total = 0;
    std::vector<Comment> list;
};

/**
 * 获取模组留言列表
 * @param modId 模组 ID
 * @param page 页码（从 1 开始）
 * @param limit 每页数量
 * @param token 用于取消请求的停止令牌
 * @return CommentListResult 包含成功/失败状态和留言列表
 */
CommentListResult fetchComments(int modId, int page = 1, int limit = 20, std::stop_token token = {});

// 提交留言的结果
struct CommentSubmitResult : api::ApiResult {
};

/**
 * 提交模组留言
 * @param modId 模组 ID
 * @param nickname 用户昵称
 * @param content 留言内容
 * @param token 用于取消请求的停止令牌
 * @return CommentSubmitResult 包含成功/失败状态和错误信息
 */
CommentSubmitResult submitComment(int modId, const std::string& nickname, const std::string& content, std::stop_token token = {});

} // namespace api::comment
