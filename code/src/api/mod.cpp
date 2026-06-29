/**
 * api/mod - 模组相关 API 请求实现
 */

#include "api/mod.hpp"
#include "api/url.hpp"
#include "api/utils.hpp"
#include "utils/http.hpp"
#include "utils/jsonResp.hpp"
#include <borealis/core/i18n.hpp>

namespace api::mod {

ModListResult fetchModList(const std::string& gameTid, int page, int limit,
                           const std::string& sort, const std::string& keyword,
                           const std::string& version, const std::string& modType,
                           std::stop_token token) {
    auto request = api::utils::makeRequest(http::Method::Get, url::mod::list(gameTid, page, limit, sort, keyword, version, modType), token);
    auto resp = http::requestToMemory(request);
    if (!api::utils::isOk(resp)) return {false, api::utils::responseErrorMessage(resp)};

    JsonResp json(resp);
    if (!json.enter("data")) return {false, api::utils::getParseErrorMessage()};

    ModListResult result;
    result.success = true;
    result.gameTid = json.getString("game_tid");
    result.gameName = json.getString("game_name");
    result.total = json.getInt("total");

    for (auto item : json.getArray("list")) {
        ModList mod;
        mod.modId         = item.getInt("mod_id");
        mod.modName       = item.getString("mod_name");
        mod.modType       = item.getString("mod_type");
        mod.modVersion    = item.getString("mod_version");
        mod.gameVersion   = item.getString("game_version");
        mod.author        = item.getString("author");
        mod.authorLink    = item.getString("author_link");
        mod.downloadCount = item.getInt("download_count");
        mod.likeCount     = item.getInt("like_count");
        mod.dislikeCount  = item.getInt("dislike_count");
        mod.uploadTime    = item.getString("upload_time");
        mod.fileCrc32     = item.getString("file_crc32");
        result.list.push_back(std::move(mod));
    }

    return result;
}

ModGameVersionsResult fetchModGameVersions(const std::string& gameTid, std::stop_token token) {
    auto request = api::utils::makeRequest(http::Method::Get, url::mod::gameVersions(gameTid), token);
    auto resp = http::requestToMemory(request);
    if (!api::utils::isOk(resp)) return {false, api::utils::responseErrorMessage(resp)};

    JsonResp json(resp);
    ModGameVersionsResult result;
    result.success = true;
    result.versions = json.getStringArray("data");
    return result;
}

ModDetailResult fetchModDetail(int modId, std::stop_token token) {
    auto request = api::utils::makeRequest(http::Method::Get, url::mod::detail(modId), token);
    auto resp = http::requestToMemory(request);
    if (!api::utils::isOk(resp)) return {false, api::utils::responseErrorMessage(resp)};

    JsonResp json(resp);
    if (!json.enter("data")) return {false, api::utils::getParseErrorMessage()};

    ModDetailResult result;
    result.success = true;
    auto& d = result.detail;
    d.modId         = json.getInt("mod_id");
    d.gameTid       = json.getString("game_tid");
    d.gameName      = json.getString("game_name");
    d.modName       = json.getString("mod_name");
    d.modType       = json.getString("mod_type");
    d.modVersion    = json.getString("mod_version");
    d.gameVersion   = json.getString("game_version");
    d.author        = json.getString("author");
    d.authorLink    = json.getString("author_link");
    d.description   = json.getString("description");
    d.fileSize      = json.getInt("file_size");
    d.uploadTime    = json.getString("upload_time");
    d.downloadCount = json.getInt("download_count");
    d.likeCount     = json.getInt("like_count");
    d.dislikeCount  = json.getInt("dislike_count");
    d.deviceLiked   = json.getBool("device_liked");
    d.deviceDisliked = json.getBool("device_disliked");
    d.fileCrc32     = json.getString("file_crc32");
    d.changelog     = json.getString("changelog");
    return result;
}

api::ApiResult toggleLike(int modId, std::stop_token token) {
    auto request = api::utils::makeRequest(http::Method::Post, url::mod::like(modId), token);
    api::utils::setTextBody(request, "", "application/x-www-form-urlencoded");
    auto resp = http::requestToMemory(request);
    if (!api::utils::isOk(resp)) return {false, api::utils::responseErrorMessage(resp)};

    JsonResp json(resp);
    if (json.getString("status") != "success") return {false, api::utils::getParseErrorMessage()};

    api::ApiResult result;
    result.success = true;
    return result;
}

api::ApiResult toggleDislike(int modId, std::stop_token token) {
    auto request = api::utils::makeRequest(http::Method::Post, url::mod::dislike(modId), token);
    api::utils::setTextBody(request, "", "application/x-www-form-urlencoded");
    auto resp = http::requestToMemory(request);
    if (!api::utils::isOk(resp)) return {false, api::utils::responseErrorMessage(resp)};

    JsonResp json(resp);
    if (json.getString("status") != "success") return {false, api::utils::getParseErrorMessage()};

    api::ApiResult result;
    result.success = true;
    return result;
}

DownloadInfoResult fetchDownloadInfo(int modId, std::stop_token token) {
    auto request = api::utils::makeRequest(http::Method::Get, url::mod::downloadInfo(modId), token);
    auto resp = http::requestToMemory(request);
    if (!api::utils::isOk(resp)) return {false, api::utils::responseErrorMessage(resp)};

    JsonResp json(resp);
    if (!json.enter("data")) return {false, api::utils::getParseErrorMessage()};

    DownloadInfoResult result;
    result.success = true;
    result.gameNameEn = json.getString("game_name_en");
    result.modNameEn  = json.getString("mod_name_en");
    return result;
}

api::ApiResult download(int modId, const std::string& path, std::function<bool(size_t total, size_t now)> progress, std::stop_token token) {
    auto resp = api::utils::downloadToFile(url::mod::download(modId), path, progress, token);
    if (api::utils::isOk(resp)) return {true, ""};
    if (resp.statusCode == 429) return {false, brls::getStr("other/api/modDownloadRateLimit")};
    return {false, brls::getStr("other/api/modDownloadFailed")};
}

ModUpdateCheckResult checkModUpdates(const std::string& gameTid, const std::string& modsJson, std::stop_token token) {
    auto request = api::utils::makeRequest(http::Method::Post, url::mod::checkUpdates(gameTid), token);
    api::utils::setTextBody(request, modsJson, "application/json");
    auto resp = http::requestToMemory(request);
    if (!api::utils::isOk(resp)) return {false, api::utils::responseErrorMessage(resp)};

    JsonResp json(resp);
    if (json.getString("status") != "success") return {false, api::utils::getParseErrorMessage()};

    if (!json.enter("data")) return {false, api::utils::getParseErrorMessage()};

    ModUpdateCheckResult result;
    result.success = true;
    result.updatedModIds = json.getIntArray("updated_mod_ids");
    return result;
}

ScreenshotResult fetchScreenshot(const std::string& gameTid, int modId, int index, std::stop_token token) {
    // 优先镜像加速
    auto resp = api::utils::downloadBytes(url::mod::screenshotMirror(gameTid, modId, index), {}, token);
    if (api::utils::isOk(resp) && !resp.body.empty()) return {true, "", std::move(resp.body)};
    if (token.stop_requested()) return {};

    // 镜像失败，回退源站
    resp = api::utils::downloadBytes(url::mod::screenshot(gameTid, modId, index), {}, token);
    if (api::utils::isOk(resp) && !resp.body.empty()) return {true, "", std::move(resp.body)};

    return {};
}

} // namespace api::mod
