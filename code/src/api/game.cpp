/**
 * api/game - 游戏相关 API 请求实现
 */

#include "api/game.hpp"
#include "api/url.hpp"
#include "utils/http.hpp"
#include "utils/jsonResp.hpp"
#include <borealis/core/i18n.hpp>

namespace api::game {

NameResult fetchName(const std::string& tid, std::stop_token token) {
    auto resp = http::get(url::game::name(tid), token);
    if (!resp.ok()) return {false, brls::getStr("other/api/networkError")};

    JsonResp json(resp);
    if (!json.enter("data")) return {false, brls::getStr("other/api/parseError")};
    if (!json.getBool("exists")) return {false, brls::getStr("other/api/gameNotFound")};

    std::string name = json.getString("name");
    if (name.empty()) return {false, brls::getStr("other/api/gameNotFound")};
    return {true, "", name};
}

GameListResult fetchGameList(int page, int limit, const std::string& keyword, std::stop_token token) {
    auto resp = http::get(url::game::list(page, limit, keyword), token);
    if (!resp.ok()) {
        JsonResp json(resp);
        std::string message = json.getString("message");
        return {false, message.empty() ? brls::getStr("other/api/networkError") : message};
    }

    JsonResp json(resp);
    if (!json.enter("data")) return {false, brls::getStr("other/api/parseError")};

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
    std::string json = "{\"tids\":" + tidsJson + ",\"returnInstalled\":" + (returnInstalled ? "true" : "false") + '}';

    auto resp = http::postJson(url::game::installed(page, limit, keyword), json, token);
    if (!resp.ok()) {
        JsonResp jr(resp);
        std::string message = jr.getString("message");
        return {false, message.empty() ? brls::getStr("other/api/networkError") : message};
    }

    JsonResp jr(resp);
    if (!jr.enter("data")) return {false, brls::getStr("other/api/parseError")};

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

} // namespace api::game
