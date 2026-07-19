/**
 * api/utils - API 层通用小工具
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <stop_token>
#include <string>
#include <vector>

#include "utils/http.hpp"

namespace api::utils {

/** @brief 判断 HTTP 响应是否为网络成功且状态码 2xx */
bool isOk(const http::Response& resp);

/** @brief 按小写 header 名读取响应头，找不到返回空字符串 */
std::string headerValue(const http::Response& resp, const std::string& name);

/** @brief 从 API 错误响应里读取 message，读不到则返回默认网络错误 */
std::string responseErrorMessage(const http::Response& resp);

/** @brief 获取 JSON 解析失败错误文本 */
std::string getParseErrorMessage();

/** @brief 将字符串原样转成 HTTP 请求体字节 */
std::vector<uint8_t> toBytes(const std::string& value);

/** @brief 创建 API 请求基础 header */
std::vector<http::Header> baseRequestHeaders();

/** @brief 创建带基础 header 的 API 请求 */
http::Request makeRequest(http::Method method, const std::string& url, std::stop_token token);

/** @brief 设置字符串请求体和对应的 Content-Type */
void setTextBody(http::Request& request, const std::string& body, const std::string& contentType);

/** @brief 下载二进制数据到内存，可附加自定义请求头 */
http::Response downloadBytes(const std::string& url, const std::vector<http::Header>& headers = {}, std::stop_token token = {});

/** @brief 下载文件到指定路径，失败时删除不完整文件 */
http::Response downloadToFile(const std::string& url, const std::string& path, std::function<bool(size_t total, size_t now)> progress = {}, std::stop_token token = {});

/** @brief 向 header 列表追加一项，name 为空时不追加 */
void addHeader(std::vector<http::Header>& headers, const std::string& name, const std::string& value);

} // namespace api::utils
