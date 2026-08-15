/**
 * api/app - 应用更新相关 API 请求实现
 */

#include "api/app.hpp"
#include "api/url.hpp"
#include "api/utils.hpp"
#include "utils/http.hpp"
#include "utils/jsonResp.hpp"

#include <utility>

namespace api::app {

LatestResult fetchLatest(std::stop_token token) {
    auto request = api::utils::makeRequest(http::Method::Get, url::app::latest(), token);
    auto resp = http::requestToMemory(request);
    if (!api::utils::isOk(resp)) return {false, api::utils::responseErrorMessage(resp)};

    JsonResp json(resp);
    if (!json.enter("data")) return {false, api::utils::getParseErrorMessage()};

    LatestResult result;
    result.success = true;
    result.tagName = json.getString("tag_name");
    result.publishTime = json.getString("publish_time");
    result.releaseNotesZh = json.getString("release_notes_zh");
    result.releaseNotesEn = json.getString("release_notes_en");
    return result;
}

VersionHistoryResult fetchVersionHistory(std::stop_token token) {
    auto request = api::utils::makeRequest(http::Method::Get, url::app::versionHistory(), token);
    auto resp = http::requestToMemory(request);
    if (!api::utils::isOk(resp)) return {false, api::utils::responseErrorMessage(resp)};

    JsonResp json(resp);
    if (!json.enter("data")) return {false, api::utils::getParseErrorMessage()};

    VersionHistoryResult result;
    result.success = true;
    for (auto item : json.getArray("list")) {
        VersionHistoryItem version;
        version.tagName = item.getString("tag_name");
        version.publishTime = item.getString("publish_time");
        version.releaseNotesZh = item.getString("release_notes_zh");
        version.releaseNotesEn = item.getString("release_notes_en");
        result.list.push_back(std::move(version));
    }
    return result;
}

api::ApiResult download(const std::string& path, std::function<bool(size_t total, size_t now)> progress, std::stop_token token) {
    auto resp = api::utils::downloadToFile(url::app::download(), path, progress, token);
    if (api::utils::isOk(resp)) return {true, ""};
    return {false, api::utils::responseErrorMessage(resp)};
}

} // namespace api::app
