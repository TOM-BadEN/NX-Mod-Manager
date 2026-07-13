/**
 * api/game - 游戏相关 API 请求实现
 */

#include "api/game.hpp"
#include "api/url.hpp"
#include "api/utils.hpp"
#include "utils/http.hpp"
#include "utils/jsonResp.hpp"
#include <borealis/core/i18n.hpp>

namespace api::game {

NameResult fetchName(const std::string& tid, std::stop_token token) {
    auto request = api::utils::makeRequest(http::Method::Get, url::game::name(tid), token);
    auto resp = http::requestToMemory(request);
    if (!api::utils::isOk(resp)) return {false, api::utils::responseErrorMessage(resp)};

    JsonResp json(resp);
    if (!json.enter("data")) return {false, api::utils::getParseErrorMessage()};
    if (!json.getBool("exists")) return {false, brls::getStr("other/api/gameNotFound")};

    std::string name = json.getString("name");
    if (name.empty()) return {false, brls::getStr("other/api/gameNotFound")};
    return {true, "", name};
}

GameListResult fetchGameList(int page, int limit, const std::string& keyword, std::stop_token token) {
    auto request = api::utils::makeRequest(http::Method::Get, url::game::list(page, limit, keyword), token);
    auto resp = http::requestToMemory(request);
    if (!api::utils::isOk(resp)) return {false, api::utils::responseErrorMessage(resp)};

    JsonResp json(resp);
    if (!json.enter("data")) return {false, api::utils::getParseErrorMessage()};

    GameListResult result;
    result.success = true;
    result.total = json.getInt("total");

    for (auto item : json.getArray("list")) {
        GameList game;
        game.gameTid    = item.getString("game_tid");
        game.gameName   = item.getString("game_name");
        game.modCount   = item.getInt("mod_count");
        game.lastUpdate = item.getString("last_update");
        result.list.push_back(std::move(game));
    }

    return result;
}

GameListResult fetchInstalledGameList(int page, int limit, const std::string& keyword, const std::string& tidsJson, bool returnInstalled, std::stop_token token) {
    std::string body = "{\"tids\":" + tidsJson + ",\"returnInstalled\":" + (returnInstalled ? "true" : "false") + '}';

    auto request = api::utils::makeRequest(http::Method::Post, url::game::installed(page, limit, keyword), token);
    api::utils::setTextBody(request, body, "application/json");
    auto resp = http::requestToMemory(request);
    if (!api::utils::isOk(resp)) return {false, api::utils::responseErrorMessage(resp)};

    JsonResp jr(resp);
    if (!jr.enter("data")) return {false, api::utils::getParseErrorMessage()};

    GameListResult result;
    result.success = true;
    result.total = jr.getInt("total");

    for (auto item : jr.getArray("list")) {
        GameList game;
        game.gameTid    = item.getString("game_tid");
        game.gameName   = item.getString("game_name");
        game.modCount   = item.getInt("mod_count");
        game.lastUpdate = item.getString("last_update");
        result.list.push_back(std::move(game));
    }

    return result;
}

static std::vector<http::Header> iconRequestHeaders(const IconCacheValidator& validator) {
    std::vector<http::Header> headers;
    if (!validator.hasLocalFile) return headers;

    if (!validator.etag.empty()) api::utils::addHeader(headers, "If-None-Match", validator.etag);
    if (!validator.lastModified.empty()) api::utils::addHeader(headers, "If-Modified-Since", validator.lastModified);
    return headers;
}

static IconResult iconResultFromResponse(http::Response& resp) {
    IconResult result;
    if (resp.networkCode != 0) return result;

    result.etag = api::utils::headerValue(resp, "etag");
    result.lastModified = api::utils::headerValue(resp, "last-modified");

    if (resp.statusCode == 304) {
        result.success = true;
        result.notModified = true;
        return result;
    }

    if (!api::utils::isOk(resp)) return {};
    if (resp.body.empty()) return {};

    result.success = true;
    result.hasData = true;
    result.data = std::move(resp.body);
    return result;
}

IconResult fetchIcon(const std::string& gameTid, const IconCacheValidator& validator, std::stop_token token) {
    auto headers = iconRequestHeaders(validator);

    auto resp = api::utils::downloadBytes(url::game::iconMirror(gameTid), headers, token);
    auto result = iconResultFromResponse(resp);
    if (result.success) return result;
    if (token.stop_requested()) return {};

    resp = api::utils::downloadBytes(url::game::icon(gameTid), headers, token);
    return iconResultFromResponse(resp);
}

} // namespace api::game
