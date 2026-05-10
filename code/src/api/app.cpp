/**
 * api/app - 应用更新相关 API 请求实现
 */

#include "api/app.hpp"
#include "api/url.hpp"
#include "utils/http.hpp"
#include "utils/jsonResp.hpp"
#include <borealis/core/i18n.hpp>

namespace api::app {

LatestResult fetchLatest(std::stop_token token) {
    auto resp = http::get(url::app::latest(), token);
    if (!resp.ok()) {
        JsonResp json(resp);
        std::string message = json.getString("message");
        return {false, message.empty() ? brls::getStr("other/api/networkError") : message};
    }

    JsonResp json(resp);
    if (!json.enter("data")) return {false, brls::getStr("other/api/parseError")};

    LatestResult result;
    result.success        = true;
    result.tagName        = json.getString("tag_name");
    result.publishTime    = json.getString("publish_time");
    result.releaseNotesZh = json.getString("release_notes_zh");
    result.releaseNotesEn = json.getString("release_notes_en");
    return result;
}

} // namespace api::app
