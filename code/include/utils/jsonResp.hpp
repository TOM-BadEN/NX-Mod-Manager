/**
 * JsonResp - HTTP 响应 JSON 解析工具
 * 基于 yyjson，RAII 自动管理内存
 * 不绑定任何固定响应格式，通用解析
 */

#pragma once

#include <string>
#include <vector>

namespace http {
    struct Response;
} // namespace http

struct yyjson_doc;
struct yyjson_val;

/** @brief JSON 数组元素的轻量包装，不持有文档所有权 */
class JsonValue {
public:
    /**
     * @brief 创建 JSON 数组元素包装
     * @param val JSON 节点，生命周期由 JsonResp 管理
     */
    JsonValue(yyjson_val* val) : m_val(val) {}

    /**
     * @brief 获取字符串值
     * @param key 键名
     * @return 字符串值，节点或键无效时返回空字符串
     */
    std::string getString(const char* key) const;

    /**
     * @brief 获取整数值
     * @param key 键名
     * @param defaultVal 默认值
     * @return 整数值，节点或键无效时返回默认值
     */
    int getInt(const char* key, int defaultVal = 0) const;

    /**
     * @brief 获取布尔值
     * @param key 键名
     * @return 布尔值，节点或键无效时返回 false
     */
    bool getBool(const char* key) const;

private:
    yyjson_val* m_val; // JSON 节点，不持有所有权
};

/** @brief HTTP 响应 JSON 文档的 RAII 解析器 */
class JsonResp {
public:
    /**
     * @brief 从 http::Response 解析 JSON
     * @param resp HTTP 响应
     */
    JsonResp(const http::Response& resp);

    /** @brief 释放 JSON 文档 */
    ~JsonResp();

    JsonResp(const JsonResp&) = delete;
    JsonResp& operator=(const JsonResp&) = delete;

    /**
     * @brief 进入子节点，后续取值都从该节点开始
     * @param key 键名
     * @return 成功进入子节点时返回 true，否则返回 false
     */
    bool enter(const char* key);

    /**
     * @brief 获取字符串值
     * @param key 键名
     * @return 字符串值，节点或键无效时返回空字符串
     */
    std::string getString(const char* key) const;

    /**
     * @brief 获取布尔值
     * @param key 键名
     * @return 布尔值，节点或键无效时返回 false
     */
    bool getBool(const char* key) const;

    /**
     * @brief 获取整数值
     * @param key 键名
     * @param defaultVal 默认值
     * @return 整数值，节点或键无效时返回默认值
     */
    int getInt(const char* key, int defaultVal = 0) const;

    /**
     * @brief 获取数组，返回每个元素的 JsonValue 包装
     * @param key 键名
     * @return JSON 数组元素包装列表，节点或键无效时返回空列表
     */
    std::vector<JsonValue> getArray(const char* key) const;

    /**
     * @brief 获取字符串数组
     * @param key 键名
     * @return 字符串数组，节点或键无效时返回空列表
     */
    std::vector<std::string> getStringArray(const char* key) const;

    /**
     * @brief 获取整数数组
     * @param key 键名
     * @return 整数数组，节点或键无效时返回空列表
     */
    std::vector<int> getIntArray(const char* key) const;

private:
    yyjson_doc* m_doc = nullptr;  // 持有的 JSON 文档
    yyjson_val* m_node = nullptr; // 当前操作节点
};
