/**
 * api/apiResult - API 结果基类
 * 所有 API 返回结果的公共部分（成功/失败状态 + 错误信息）
 */

#pragma once

#include <string>

namespace api {

// 所有 API 结果的公共基类
struct ApiResult {
    bool success = false;   // 请求是否成功
    std::string error;      // 失败时的错误信息
};

} // namespace api
