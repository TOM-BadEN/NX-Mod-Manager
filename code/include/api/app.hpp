/**
 * api/app - 应用更新相关 API 请求
 * 封装 HTTP 请求 + JSON 解析，返回干净的业务数据
 */

#pragma once

#include "api/apiResult.hpp"
#include <cstddef>
#include <functional>
#include <string>
#include <stop_token>

namespace api::app {

// 获取最新版本信息的结果
struct LatestResult : api::ApiResult {
    std::string tagName;          // 版本号（如 "v3.2.0"）
    std::string publishTime;      // 发布时间（ISO 8601）
    std::string releaseNotesZh;   // 中文更新日志
    std::string releaseNotesEn;   // 英文更新日志
};

/**
 * 获取最新版本信息
 * @param token 用于取消请求的停止令牌
 * @return LatestResult 包含成功/失败状态和版本信息
 */
LatestResult fetchLatest(std::stop_token token = {});

/**
 * 下载最新版本文件到指定路径
 * @param path 保存路径
 * @param progress 进度回调 (total, now) → 返回 false 可中断
 * @param token 用于取消请求的停止令牌
 * @return ApiResult 包含成功/失败状态和错误信息
 */
api::ApiResult download(const std::string& path, std::function<bool(size_t total, size_t now)> progress = {}, std::stop_token token = {});

} // namespace api::app
