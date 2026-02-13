/**
 * JsonFile - JSON 文件读写封装
 */

#include "utils/jsonFile.hpp"
#include "yyjson.h"
#include <cstdio>
#include <cstring>
#include <vector>

JsonFile::JsonFile() = default;

JsonFile::~JsonFile() {
    if (m_doc) yyjson_mut_doc_free(m_doc);
}

// 确保 m_doc 存在（空文档 + 空根对象）
yyjson_mut_doc* JsonFile::ensureDoc() {
    if (!m_doc) {
        m_doc = yyjson_mut_doc_new(nullptr);
        if (m_doc) {
            yyjson_mut_val* root = yyjson_mut_obj(m_doc);
            if (root) yyjson_mut_doc_set_root(m_doc, root);
        }
    }
    return m_doc;
}

// 查找 rootKey 对应的子对象，找不到返回 nullptr
void* JsonFile::findRootObj(const std::string& rootKey) {
    if (!m_doc) return nullptr;
    yyjson_mut_val* root = yyjson_mut_doc_get_root(m_doc);
    if (!root || !yyjson_mut_is_obj(root)) return nullptr;
    yyjson_mut_val* obj = yyjson_mut_obj_get(root, rootKey.c_str());
    if (!obj || !yyjson_mut_is_obj(obj)) return nullptr;
    return obj;
}

// 查找或创建 rootKey 对应的子对象
void* JsonFile::findOrCreateRootObj(const std::string& rootKey) {
    if (!ensureDoc()) return nullptr;
    yyjson_mut_val* root = yyjson_mut_doc_get_root(m_doc);
    if (!root) return nullptr;

    yyjson_mut_val* obj = yyjson_mut_obj_get(root, rootKey.c_str());
    if (obj && yyjson_mut_is_obj(obj)) return obj;

    // 不存在或不是对象，创建新的
    obj = yyjson_mut_obj(m_doc);
    if (!obj) return nullptr;
    yyjson_mut_val* keyVal = yyjson_mut_strcpy(m_doc, rootKey.c_str());
    if (!keyVal) return nullptr;
    yyjson_mut_obj_put(root, keyVal, obj);
    return obj;
}

// 读取文件到内存
bool JsonFile::load(const std::string& path) {
    m_path = path;

    // 释放旧文档
    if (m_doc) {
        yyjson_mut_doc_free(m_doc);
        m_doc = nullptr;
    }

    FILE* file = fopen(path.c_str(), "rb");
    if (!file) return false;

    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (fileSize <= 0) {
        fclose(file);
        return false;
    }

    std::vector<char> buffer(fileSize + 1);
    size_t readSize = fread(buffer.data(), 1, fileSize, file);
    fclose(file);

    if (readSize != static_cast<size_t>(fileSize)) return false;
    buffer[fileSize] = '\0';

    // 解析为不可变文档，再转为可变文档
    yyjson_doc* doc = yyjson_read(buffer.data(), fileSize, 0);
    if (!doc) return false;

    m_doc = yyjson_doc_mut_copy(doc, nullptr);
    yyjson_doc_free(doc);

    return m_doc != nullptr;
}

// 写回文件
bool JsonFile::save() {
    if (m_path.empty()) return false;
    if (!ensureDoc()) return false;

    const char* jsonStr = yyjson_mut_write(m_doc, YYJSON_WRITE_PRETTY, nullptr);
    if (!jsonStr) return false;

    FILE* file = fopen(m_path.c_str(), "wb");
    if (!file) {
        free((void*)jsonStr);
        return false;
    }

    size_t jsonLen = strlen(jsonStr);
    size_t written = fwrite(jsonStr, 1, jsonLen, file);
    fclose(file);
    free((void*)jsonStr);

    return written == jsonLen;
}

// 读取字符串
std::string JsonFile::getString(const std::string& rootKey, const std::string& key, const std::string& defaultVal) {
    yyjson_mut_val* obj = static_cast<yyjson_mut_val*>(findRootObj(rootKey));
    if (!obj) return defaultVal;

    yyjson_mut_val* val = yyjson_mut_obj_get(obj, key.c_str());
    if (!val || !yyjson_mut_is_str(val)) return defaultVal;

    const char* str = yyjson_mut_get_str(val);
    return str ? std::string(str) : defaultVal;
}

// 读取布尔值
bool JsonFile::getBool(const std::string& rootKey, const std::string& key, bool defaultVal) {
    yyjson_mut_val* obj = static_cast<yyjson_mut_val*>(findRootObj(rootKey));
    if (!obj) return defaultVal;

    yyjson_mut_val* val = yyjson_mut_obj_get(obj, key.c_str());
    if (!val || !yyjson_mut_is_bool(val)) return defaultVal;

    return yyjson_mut_get_bool(val);
}

// 写入字符串
void JsonFile::setString(const std::string& rootKey, const std::string& key, const std::string& value) {
    yyjson_mut_val* obj = static_cast<yyjson_mut_val*>(findOrCreateRootObj(rootKey));
    if (!obj) return;

    yyjson_mut_val* keyVal = yyjson_mut_strcpy(m_doc, key.c_str());
    yyjson_mut_val* valVal = yyjson_mut_strcpy(m_doc, value.c_str());
    if (keyVal && valVal) yyjson_mut_obj_put(obj, keyVal, valVal);
}

// 写入布尔值
void JsonFile::setBool(const std::string& rootKey, const std::string& key, bool value) {
    yyjson_mut_val* obj = static_cast<yyjson_mut_val*>(findOrCreateRootObj(rootKey));
    if (!obj) return;

    yyjson_mut_val* keyVal = yyjson_mut_strcpy(m_doc, key.c_str());
    yyjson_mut_val* valVal = yyjson_mut_bool(m_doc, value);
    if (keyVal && valVal) yyjson_mut_obj_put(obj, keyVal, valVal);
}

// 查询根键是否存在
bool JsonFile::hasRootKey(const std::string& rootKey) {
    return findRootObj(rootKey) != nullptr;
}

// 删除根键
void JsonFile::removeRootKey(const std::string& rootKey) {
    if (!m_doc) return;
    yyjson_mut_val* root = yyjson_mut_doc_get_root(m_doc);
    if (!root || !yyjson_mut_is_obj(root)) return;
    yyjson_mut_obj_remove_str(root, rootKey.c_str());
}
