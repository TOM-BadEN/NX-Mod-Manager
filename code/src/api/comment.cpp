/**
 * api/comment - 留言相关 API 请求实现
 */

#include "api/comment.hpp"
#include "api/url.hpp"
#include "api/utils.hpp"
#include "utils/http.hpp"
#include "utils/jsonResp.hpp"

namespace api::comment {

CommentListResult fetchComments(int modId, int page, int limit, std::stop_token token) {
    auto request = api::utils::makeRequest(http::Method::Get, url::comment::list(modId, page, limit), token);
    auto resp = http::requestToMemory(request);
    if (!api::utils::isOk(resp)) return {false, api::utils::responseErrorMessage(resp)};

    JsonResp json(resp);
    if (!json.enter("data")) return {false, api::utils::getParseErrorMessage()};

    CommentListResult result;
    result.success = true;
    result.total = json.getInt("total");

    for (auto item : json.getArray("list")) {
        Comment c;
        c.commentId  = item.getInt("comment_id");
        c.nickname   = item.getString("nickname");
        c.content    = item.getString("content");
        c.createTime = item.getString("create_time");
        result.list.push_back(std::move(c));
    }
    return result;
}

CommentSubmitResult submitComment(int modId, const std::string& nickname, const std::string& content, std::stop_token token) {
                                    
    std::string body = "nickname=" + http::escape(nickname) + "&content=" + http::escape(content);

    auto request = api::utils::makeRequest(http::Method::Post, url::comment::submit(modId), token);
    api::utils::setTextBody(request, body, "application/x-www-form-urlencoded");
    auto resp = http::requestToMemory(request);
    if (!api::utils::isOk(resp)) return {false, api::utils::responseErrorMessage(resp)};

    JsonResp json(resp);
    if (json.getString("status") != "success") return {false, api::utils::getParseErrorMessage()};

    CommentSubmitResult result;
    result.success = true;
    return result;
}

} // namespace api::comment
