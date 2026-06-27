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

IconResult fetchIcon(const std::string& gameTid, std::stop_token token) {
    // 优先镜像加速
    auto resp = api::utils::downloadBytes(url::game::iconMirror(gameTid), {}, token);
    if (api::utils::isOk(resp) && !resp.body.empty()) return {true, "", std::move(resp.body)};
    if (token.stop_requested()) return {};

    // 镜像失败，回退源站
    resp = api::utils::downloadBytes(url::game::icon(gameTid), {}, token);
    if (api::utils::isOk(resp) && !resp.body.empty()) return {true, "", std::move(resp.body)};

    return {};
}

} // namespace api::game
