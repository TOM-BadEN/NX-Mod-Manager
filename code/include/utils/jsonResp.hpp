/**
 * JsonResp - HTTP 响应 JSON 解析工具
 * 基于 yyjson，RAII 自动管理内存
 * 不绑定任何固定响应格式，通用解析
 */

#pragma once

#include <string>
#include <vector>

namespace http { struct Response; }  // 前向声明
struct yyjson_doc;
struct yyjson_val;

// JSON 数组元素的轻量包装（不持有文档所有权，生命周期由 JsonResp 管理）
class JsonValue {
public:
    JsonValue(yyjson_val* val) : m_val(val) {}

    /**
     * @brief 获取字符串值
     * @param key 键名
     */
    std::string getString(const char* key) const;

    /**
     * @brief 获取整数值
     * @param key 键名
     * @param defaultVal 默认值
     */
    int getInt(const char* key, int defaultVal = 0) const;

    /**
     * @brief 获取布尔值
     * @param key 键名
     */
    bool getBool(const char* key) const;

private:
    yyjson_val* m_val;
};

class JsonResp {
public:
    /**
     * @brief 从 http::Response 解析 JSON
     * @param resp HTTP 响应
     */
    JsonResp(const http::Response& resp);
    ~JsonResp();

    JsonResp(const JsonResp&) = delete;
    JsonResp& operator=(const JsonResp&) = delete;

    /**
     * @brief 进入子节点，后续取值都从该节点开始
     * @param key 键名
     */
    bool enter(const char* key);

    /**
     * @brief 获取字符串值
     * @param key 键名
     */
    std::string getString(const char* key) const;

    /**
     * @brief 获取布尔值
     * @param key 键名
     */
    bool getBool(const char* key) const;

    /**
     * @brief 获取整数值
     * @param key 键名
     * @param defaultVal 默认值
     */
    int getInt(const char* key, int defaultVal = 0) const;

    /**
     * @brief 获取数组，返回每个元素的 JsonValue 包装
     * @param key 键名
     */
    std::vector<JsonValue> getArray(const char* key) const;

    /**
     * @brief 获取字符串数组
     * @param key 键名
     */
    std::vector<std::string> getStringArray(const char* key) const;

    /**
     * @brief 获取整数数组
     * @param key 键名
     */
    std::vector<int> getIntArray(const char* key) const;

private:
    yyjson_doc* m_doc = nullptr;
    yyjson_val* m_node = nullptr;  // 当前操作节点
};
