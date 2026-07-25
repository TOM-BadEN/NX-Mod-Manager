/**
 * api/utils - API 层通用小工具
 */

#pragma once

#include "utils/http.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <stop_token>
#include <string>
#include <vector>

namespace api::utils {

/**
 * @brief 判断 HTTP 响应是否为网络成功且状态码 2xx
 * @param resp HTTP 响应
 * @return 网络层成功且状态码为 2xx 时返回 true，否则返回 false
 */
bool isOk(const http::Response& resp);

/**
 * @brief 按小写 header 名读取响应头
 * @param resp HTTP 响应
 * @param name 小写响应头名称
 * @return 响应头值，找不到时返回空字符串
 */
std::string headerValue(const http::Response& resp, const std::string& name);

/**
 * @brief 从 API 错误响应里读取 message
 * @param resp HTTP 响应
 * @return API 错误文本，读取不到时返回默认网络错误
 */
std::string responseErrorMessage(const http::Response& resp);

/**
 * @brief 获取 JSON 解析失败错误文本
 * @return 本地化的 JSON 解析失败错误文本
 */
std::string getParseErrorMessage();

/**
 * @brief 将字符串原样转成 HTTP 请求体字节
 * @param value 原始字符串
 * @return 字符串对应的原始字节
 */
std::vector<uint8_t> toBytes(const std::string& value);

/**
 * @brief 创建 API 请求基础 header
 * @return 包含设备 ID 和客户端版本的请求头列表
 */
std::vector<http::Header> baseRequestHeaders();

/**
 * @brief 创建带基础 header 的 API 请求
 * @param method HTTP 请求方法
 * @param url 请求地址
 * @param token 取消令牌
 * @return API 请求参数
 */
http::Request makeRequest(http::Method method, const std::string& url, std::stop_token token);

/**
 * @brief 设置字符串请求体和对应的 Content-Type
 * @param request 待设置的请求参数
 * @param body 字符串请求体
 * @param contentType 请求体内容类型
 */
void setTextBody(http::Request& request, const std::string& body, const std::string& contentType);

/**
 * @brief 下载二进制数据到内存，可附加自定义请求头
 * @param url 下载地址
 * @param headers 自定义请求头
 * @param token 取消令牌
 * @return HTTP 响应结果
 */
http::Response downloadBytes(const std::string& url, const std::vector<http::Header>& headers = {}, std::stop_token token = {});

/**
 * @brief 下载文件到指定路径，失败时删除不完整文件
 * @param url 下载地址
 * @param path 保存路径
 * @param progress 下载进度回调
 * @param token 取消令牌
 * @return HTTP 响应结果
 */
http::Response downloadToFile(const std::string& url, const std::string& path, std::function<bool(size_t total, size_t now)> progress = {}, std::stop_token token = {});

/**
 * @brief 向 header 列表追加一项，name 为空时不追加
 * @param headers 请求头列表
 * @param name 请求头名称
 * @param value 请求头值
 */
void addHeader(std::vector<http::Header>& headers, const std::string& name, const std::string& value);

} // namespace api::utils
