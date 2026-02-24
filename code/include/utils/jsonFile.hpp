/**
 * JsonFile - JSON 文件读写封装
 * 基于 yyjson，提供两层嵌套结构的通用读写接口
 * 不包含任何业务逻辑，纯 JSON 操作
 */

#pragma once

#include <string>

// 前向声明 yyjson 类型，避免暴露 yyjson.h
struct yyjson_mut_doc;

class JsonFile {
public:
    JsonFile();
    ~JsonFile();

    // 禁止拷贝
    JsonFile(const JsonFile&) = delete;
    JsonFile& operator=(const JsonFile&) = delete;

    // 文件操作
    bool load(const std::string& path);
    bool save();

    // 读取（rootKey → key，找不到返回默认值）
    std::string getString(const std::string& rootKey, const std::string& key, const std::string& defaultVal = "");
    bool getBool(const std::string& rootKey, const std::string& key, bool defaultVal = false);

    // 写入（rootKey 不存在时自动创建）
    void setString(const std::string& rootKey, const std::string& key, const std::string& value);
    void setBool(const std::string& rootKey, const std::string& key, bool value);

    // 查询
    bool hasRootKey(const std::string& rootKey);

    // 删除
    void removeRootKey(const std::string& rootKey);
    void removeKey(const std::string& rootKey, const std::string& key);

private:
    std::string m_path;
    yyjson_mut_doc* m_doc = nullptr;

    // 获取或创建 rootKey 对应的对象
    yyjson_mut_doc* ensureDoc();
    void* findRootObj(const std::string& rootKey);
    void* findOrCreateRootObj(const std::string& rootKey);
};
