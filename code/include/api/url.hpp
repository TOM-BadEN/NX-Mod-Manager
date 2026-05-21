/**
 * api/url - API URL 定义
 * 集中管理所有后端 API 的 URL 拼接
 *
 * 此文件为模板文件，实际的 url.hpp 未开源。
 * 服务器由作者个人承担，运营成本高昂且抗压能力有限，
 * 公开接口地址将面临滥用与恶意攻击的风险，因此不纳入开源范围。
 * 如需自行搭建后端服务，请将此文件复制为 url.hpp 并实现各命名空间下的接口函数。
 *
 * This file is a template. The actual url.hpp is not open-sourced.
 * The server is personally funded by the author, with high operating costs and limited capacity.
 * Exposing the API endpoints would risk abuse and malicious attacks,
 * so they are excluded from the open-source release.
 * If you wish to set up your own backend, copy this file as url.hpp and implement
 * the API functions under each namespace.
 */

#pragma once

#include <string>
#include <fmt/core.h>
#include <borealis/core/application.hpp>

namespace api::url {

inline const std::string base = "http://url";

inline std::string getLang() { return "lang"; }

namespace game {
    inline std::string list(int page = 1, int limit = 20, const std::string& keyword = "") { return base; }
    inline std::string name(const std::string& tid) { return base; }
    inline std::string installed(int page = 1, int limit = 20, const std::string& keyword = "") { return base; }
    inline std::string icon(const std::string& tid) { return base; }
}

namespace mod {
    inline std::string gameVersions(const std::string& gameTid) { return base; }
    inline std::string list(const std::string& gameTid, int page = 1, int limit = 20, const std::string& sort = "latest", const std::string& keyword = "", const std::string& version = "", const std::string& modType = "") { return base; }
    inline std::string detail(int modId) { return base; }
    inline std::string like(int modId) { return base; }
    inline std::string dislike(int modId) { return base; }
    inline std::string downloadInfo(int modId) { return base; }
    inline std::string download(int modId) { return base; }
    inline std::string screenshot(const std::string& gameTid, int modId, int index) { return base; }
}

namespace app {
    inline std::string latest() { return base; }
    inline std::string download() { return base; }
}

namespace comment {
    inline std::string list(int modId, int page = 1, int limit = 20) { return base; }
    inline std::string submit(int modId) { return base; }
}

} // namespace api::url
