/**
 * api/comment - 留言相关 API 请求实现
 */

#include "api/comment.hpp"
#include "api/url.hpp"
#include "utils/http.hpp"
#include "utils/jsonResp.hpp"
#include <borealis/core/i18n.hpp>

namespace api::comment {

CommentListResult fetchComments(int modId, int page, int limit, std::stop_token token) {
    auto resp = http::get(url::comment::list(modId, page, limit), token);
    if (!resp.ok()) {
        JsonResp json(resp);
        std::string message = json.getString("message");
        return {false, message.empty() ? brls::getStr("other/api/networkError") : message};
    }

    JsonResp json(resp);
    if (!json.enter("data")) return {false, brls::getStr("other/api/parseError")};

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

    auto resp = http::post(url::comment::submit(modId), body, token);
    if (!resp.ok()) {
        JsonResp json(resp);
        std::string message = json.getString("message");
        return {false, message.empty() ? brls::getStr("other/api/networkError") : message};
    }

    JsonResp json(resp);
    if (json.getString("status") != "success") return {false, brls::getStr("other/api/parseError")};

    CommentSubmitResult result;
    result.success = true;
    return result;
}

} // namespace api::comment
