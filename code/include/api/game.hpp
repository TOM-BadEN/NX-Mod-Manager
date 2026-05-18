/**
 * api/game - 游戏相关 API 请求
 * 封装 HTTP 请求 + JSON 解析，返回干净的业务数据
 */

#pragma once

#include "api/apiResult.hpp"
#include <string>
#include <vector>
#include <stop_token>

namespace api::game {

struct NameResult : api::ApiResult {
    std::string name;       // 获取到的游戏名称
};

/**
 * 根据 TID 获取游戏名称
 * @param tid 游戏 TID
 * @param token 用于取消请求的停止令牌
 * @return NameResult 包含成功/失败状态和游戏名称
 */
NameResult fetchName(const std::string& tid, std::stop_token token = {});

// 商店游戏列表中的单个游戏条目
struct GameList {
    std::string gameTid;      // 游戏 TID
    std::string gameName;     // 游戏名称（多语言，由服务器根据 lang 参数返回）
    int modCount = 0;         // 该游戏的 MOD 数量
    std::string lastUpdate;   // 最后更新时间（格式：YYYY-MM-DD HH:MM:SS）
    int iconId = 0;           // NVG 纹理 ID（0 表示未加载）
    bool installed = false;   // 本机是否已安装该游戏
};

// 获取商店游戏列表的结果
struct GameListResult : api::ApiResult {
    int total = 0;                // 服务器上的游戏总数（用于判断是否还有更多数据）
    std::vector<GameList> list;   // 当前页的游戏列表
};

/**
 * 获取商店游戏列表
 * @param page 页码（从 1 开始）
 * @param limit 每页数量
 * @param keyword 搜索关键词（空字符串表示不搜索）
 * @param token 用于取消请求的停止令牌
 * @return GameListResult 包含成功/失败状态和游戏列表
 */
GameListResult fetchGameList(int page = 1, int limit = 20, const std::string& keyword = "", std::stop_token token = {});

/**
 * 获取已安装/未安装游戏列表（POST 请求，带 TID 过滤）
 * @param page 页码（从 1 开始）
 * @param limit 每页数量
 * @param keyword 搜索关键词（空字符串表示不搜索）
 * @param tidsJson 预拼好的 TID JSON 数组字符串，如 ["xxx","yyy"]
 * @param returnInstalled true=返回已安装，false=返回未安装
 * @param token 用于取消请求的停止令牌
 * @return GameListResult 包含成功/失败状态和游戏列表
 */
GameListResult fetchInstalledGameList(int page, int limit, const std::string& keyword, const std::string& tidsJson, bool returnInstalled, std::stop_token token = {});

} // namespace api::game
