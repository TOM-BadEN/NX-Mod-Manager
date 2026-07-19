/**
 * http - HTTP/HTTPS 网络请求工具
 * 基于 libcurl 封装，提供同步的内存请求和流式请求
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

/**
 * @brief 临时暂停 HTTP，并释放 curl 复用资源
 *
 * 这是给 socketExit()/socketInitialize() 前使用的生命周期接口，不是通用线程同步屏障。
 * 调用方必须保证此时没有正在执行、也没有即将开始的 HTTP 请求；否则 suspend() 可能清理
 * 正在被 curl_easy_perform() 使用的 easy handle。
 */
void suspend();

/**
 * @brief socket 重新初始化后恢复 HTTP 复用资源
 *
 * 这是给 socket 重新初始化完成后使用的生命周期接口。当前实现只恢复 share/easy handle
 * 后续创建能力，不会协调或等待其他线程中的请求。
 */
void resume();

/** @brief HTTP 请求方法 */
enum class Method {
    Get,
    Post,
};

/** @brief HTTP 头字段 */
struct Header {
    std::string name;
    std::string value;
};

/** @brief HTTP 请求参数 */
struct Request {
    Method method = Method::Get;                              // 请求方法
    std::string url;                                          // 请求地址
    std::vector<Header> headers;                              // 请求头
    std::vector<uint8_t> body;                                // 请求体（原始字节）
    std::stop_token token = {};                               // 取消令牌
    std::function<bool(size_t total, size_t now)> progress;   // 进度回调，返回 false 可中断
};

/** @brief HTTP 响应结果，只描述网络事实，不做业务成功判断 */
struct Response {
    long statusCode = 0;                 // HTTP 状态码（200、304、404 等），0 表示没有拿到响应
    int networkCode = 0;                 // libcurl 结果码，0 表示网络层成功
    bool cancelled = false;              // 是否因 stop_token 取消
    std::string error;                   // 网络层错误文本
    std::vector<Header> headers;         // 响应头，name 统一小写
    std::vector<uint8_t> body;           // 响应体（原始字节）

    /** @brief 将 body 转为字符串（用于 JSON/文本响应） */
    std::string text() const { return {body.begin(), body.end()}; }
};

/**
 * @brief 对字符串做 URL 编码
 * @param value 原始字符串
 */
std::string escape(const std::string& value);

/**
 * @brief 执行请求，响应体完整返回到内存
 * @param request 请求参数
 */
Response requestToMemory(const Request& request);

/**
 * @brief 执行请求，响应体按块交给调用方处理
 * @param request 请求参数
 * @param onData 数据回调，返回 false 可中断
 */
Response requestStream(const Request& request, const std::function<bool(const uint8_t* data, size_t size)>& onData);

} // namespace http
