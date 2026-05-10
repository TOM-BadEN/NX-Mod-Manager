/**
 * http - HTTP/HTTPS 网络请求工具
 * 基于 libcurl 封装，提供同步的 GET/POST 请求、图片下载、文件下载
 * 所有函数均为阻塞调用，需配合 async 使用
 */

#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include <stop_token>

namespace http {

/** @brief libcurl 全局初始化（必须在单线程环境下调用） */
void init();

/** @brief libcurl 全局清理 */
void cleanup();

struct Response {
    long code;                  // HTTP 状态码（200、404 等），0 表示请求失败
    std::vector<uint8_t> body;  // 响应体（原始字节）

    bool ok() const { return code >= 200 && code < 300; }

    /** @brief 将 body 转为字符串（用于 JSON/文本响应） */
    std::string text() const { return {body.begin(), body.end()}; }
};

/** @brief 进度回调，返回 false 可中断下载 */
using ProgressCallback = std::function<bool(size_t total, size_t now)>;

/**
 * @brief GET 请求，响应内容返回到内存
 * @param url 请求地址
 * @param token 取消令牌
 */
Response get(const std::string& url, std::stop_token token = {});

/**
 * @brief 对字符串做 URL 编码
 * @param value 原始字符串
 */
std::string escape(const std::string& value);

/**
 * @brief POST 请求，响应内容返回到内存
 * @param url 请求地址
 * @param body 请求体
 * @param token 取消令牌
 */
Response post(const std::string& url, const std::string& body = "", std::stop_token token = {});

/**
 * @brief POST JSON 请求，Content-Type 为 application/json
 * @param url 请求地址
 * @param json JSON 字符串
 * @param token 取消令牌
 */
Response postJson(const std::string& url, const std::string& json, std::stop_token token = {});

/**
 * @brief 下载图片到内存（用于 NVG 纹理创建）
 * @param url 图片地址
 * @param token 取消令牌
 */
Response downloadImage(const std::string& url, std::stop_token token = {});

/**
 * @brief 下载文件到 SD 卡指定路径
 * @param url 下载地址
 * @param path 目标文件路径
 * @param onProgress 进度回调
 * @param token 取消令牌
 */
Response downloadToFile(const std::string& url, const std::string& path,
                        ProgressCallback onProgress = nullptr,
                        std::stop_token token = {});

} // namespace http
