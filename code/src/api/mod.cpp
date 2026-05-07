/**
 * api/mod - 模组相关 API 请求实现
 */

#include "api/mod.hpp"
#include "api/url.hpp"
#include "utils/http.hpp"
#include "utils/jsonResp.hpp"
#include <borealis/core/i18n.hpp>

namespace api::mod {

ModListResult fetchModList(const std::string& gameTid, int page, int limit,
                           const std::string& sort, const std::string& keyword,
                           const std::string& version, const std::string& modType,
                           std::stop_token token) {
    auto resp = http::get(url::mod::list(gameTid, page, limit, sort, keyword, version, modType), token);
    if (!resp.ok()) {
        JsonResp json(resp);
        std::string message = json.getString("message");
        return {false, message.empty() ? brls::getStr("other/api/networkError") : message};
    }

    JsonResp json(resp);
    if (!json.enter("data")) return {false, brls::getStr("other/api/parseError")};

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
        result.list.push_back(std::move(mod));
    }

    return result;
}

ModGameVersionsResult fetchModGameVersions(const std::string& gameTid, std::stop_token token) {
    auto resp = http::get(url::mod::gameVersions(gameTid), token);
    if (!resp.ok()) {
        JsonResp json(resp);
        std::string message = json.getString("message");
        return {false, message.empty() ? brls::getStr("other/api/networkError") : message};
    }

    JsonResp json(resp);
    ModGameVersionsResult result;
    result.success = true;
    result.versions = json.getStringArray("data");
    return result;
}

ModDetailResult fetchModDetail(int modId, std::stop_token token) {
    auto resp = http::get(url::mod::detail(modId), token);
    if (!resp.ok()) {
        JsonResp json(resp);
        std::string message = json.getString("message");
        return {false, message.empty() ? brls::getStr("other/api/networkError") : message};
    }

    JsonResp json(resp);
    if (!json.enter("data")) return {false, brls::getStr("other/api/parseError")};

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
    return result;
}

api::ApiResult toggleLike(int modId, std::stop_token token) {
    auto resp = http::post(url::mod::like(modId), "", token);
    if (!resp.ok()) {
        JsonResp json(resp);
        std::string message = json.getString("message");
        return {false, message.empty() ? brls::getStr("other/api/networkError") : message};
    }

    JsonResp json(resp);
    if (json.getString("status") != "success") return {false, brls::getStr("other/api/parseError")};

    api::ApiResult result;
    result.success = true;
    return result;
}

api::ApiResult toggleDislike(int modId, std::stop_token token) {
    auto resp = http::post(url::mod::dislike(modId), "", token);
    if (!resp.ok()) {
        JsonResp json(resp);
        std::string message = json.getString("message");
        return {false, message.empty() ? brls::getStr("other/api/networkError") : message};
    }

    JsonResp json(resp);
    if (json.getString("status") != "success") return {false, brls::getStr("other/api/parseError")};

    api::ApiResult result;
    result.success = true;
    return result;
}

DownloadInfoResult fetchDownloadInfo(int modId, std::stop_token token) {
    auto resp = http::get(url::mod::downloadInfo(modId), token);
    if (!resp.ok()) {
        JsonResp json(resp);
        std::string message = json.getString("message");
        return {false, message.empty() ? brls::getStr("other/api/networkError") : message};
    }

    JsonResp json(resp);
    if (!json.enter("data")) return {false, brls::getStr("other/api/parseError")};

    DownloadInfoResult result;
    result.success = true;
    result.gameNameEn = json.getString("game_name_en");
    result.modNameEn  = json.getString("mod_name_en");
    return result;
}

} // namespace api::mod
