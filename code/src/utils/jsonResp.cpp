/**
 * JsonResp - HTTP 响应 JSON 解析实现
 */

#include "utils/jsonResp.hpp"
#include "utils/http.hpp"
#include "yyjson.h"

JsonResp::JsonResp(const http::Response& resp) {
    m_doc = yyjson_read(reinterpret_cast<const char*>(resp.body.data()), resp.body.size(), 0);
    if (m_doc) m_node = yyjson_doc_get_root(m_doc);
}

JsonResp::~JsonResp() {
    if (m_doc) yyjson_doc_free(m_doc);
}

bool JsonResp::enter(const char* key) {
    if (!m_node) return false;
    yyjson_val* child = yyjson_obj_get(m_node, key);
    if (!child) return false;
    m_node = child;
    return true;
}

std::string JsonResp::getString(const char* key) const {
    if (!m_node) return "";
    yyjson_val* val = yyjson_obj_get(m_node, key);
    const char* str = val ? yyjson_get_str(val) : nullptr;
    return str ? std::string(str) : "";
}

bool JsonResp::getBool(const char* key) const {
    if (!m_node) return false;
    yyjson_val* val = yyjson_obj_get(m_node, key);
    return val ? yyjson_get_bool(val) : false;
}

int JsonResp::getInt(const char* key, int defaultVal) const {
    if (!m_node) return defaultVal;
    yyjson_val* val = yyjson_obj_get(m_node, key);
    return val ? (int)yyjson_get_int(val) : defaultVal;
}

std::vector<JsonValue> JsonResp::getArray(const char* key) const {
    std::vector<JsonValue> result;
    if (!m_node) return result;
    yyjson_val* arr = yyjson_obj_get(m_node, key);
    if (!arr || !yyjson_is_arr(arr)) return result;

    size_t idx, max;
    yyjson_val* item;
    yyjson_arr_foreach(arr, idx, max, item) {
        result.emplace_back(item);
    }
    return result;
}

std::vector<std::string> JsonResp::getStringArray(const char* key) const {
    std::vector<std::string> result;
    if (!m_node) return result;
    yyjson_val* arr = yyjson_obj_get(m_node, key);
    if (!arr || !yyjson_is_arr(arr)) return result;

    size_t idx, max;
    yyjson_val* item;
    yyjson_arr_foreach(arr, idx, max, item) {
        const char* str = yyjson_get_str(item);
        if (str) result.emplace_back(str);
    }
    return result;
}

std::vector<int> JsonResp::getIntArray(const char* key) const {
    std::vector<int> result;
    if (!m_node) return result;
    yyjson_val* arr = yyjson_obj_get(m_node, key);
    if (!arr || !yyjson_is_arr(arr)) return result;

    size_t idx, max;
    yyjson_val* item;
    yyjson_arr_foreach(arr, idx, max, item) {
        result.push_back(static_cast<int>(yyjson_get_int(item)));
    }
    return result;
}

// ── JsonValue ──

std::string JsonValue::getString(const char* key) const {
    if (!m_val) return "";
    yyjson_val* val = yyjson_obj_get(m_val, key);
    const char* str = val ? yyjson_get_str(val) : nullptr;
    return str ? std::string(str) : "";
}

int JsonValue::getInt(const char* key, int defaultVal) const {
    if (!m_val) return defaultVal;
    yyjson_val* val = yyjson_obj_get(m_val, key);
    return val ? (int)yyjson_get_int(val) : defaultVal;
}

bool JsonValue::getBool(const char* key) const {
    if (!m_val) return false;
    yyjson_val* val = yyjson_obj_get(m_val, key);
    return val ? yyjson_get_bool(val) : false;
}
