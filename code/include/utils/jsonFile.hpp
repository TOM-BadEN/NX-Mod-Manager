/**
 * JsonFile - JSON 文件读写封装
 * 基于 yyjson，提供两层嵌套结构的通用读写接口
 * 不包含任何业务逻辑，纯 JSON 操作
 */

#pragma once

#include <string>
#include <vector>

// 前向声明 yyjson 类型，避免暴露 yyjson.h
struct yyjson_mut_doc;

class JsonFile {
public:
    JsonFile();
    ~JsonFile();

    JsonFile(const JsonFile&) = delete;
    JsonFile& operator=(const JsonFile&) = delete;

    /**
     * @brief 从文件加载 JSON
     * @param path 文件路径
     */
    bool load(const std::string& path);

    /** @brief 保存到文件 */
    bool save();

    /**
     * @brief 读取字符串（rootKey → key）
     * @param rootKey 根键
     * @param key 子键
     * @param defaultVal 默认值
     */
    std::string getString(const std::string& rootKey, const std::string& key, const std::string& defaultVal = "");

    /**
     * @brief 读取布尔值（rootKey → key）
     * @param rootKey 根键
     * @param key 子键
     * @param defaultVal 默认值
     */
    bool getBool(const std::string& rootKey, const std::string& key, bool defaultVal = false);

    /**
     * @brief 读取整数（rootKey → key）
     * @param rootKey 根键
     * @param key 子键
     * @param defaultVal 默认值
     */
    int getInt(const std::string& rootKey, const std::string& key, int defaultVal = 0);

    /**
     * @brief 写入字符串（rootKey 不存在时自动创建）
     * @param rootKey 根键
     * @param key 子键
     * @param value 值
     */
    void setString(const std::string& rootKey, const std::string& key, const std::string& value);

    /**
     * @brief 写入布尔值
     * @param rootKey 根键
     * @param key 子键
     * @param value 值
     */
    void setBool(const std::string& rootKey, const std::string& key, bool value);

    /**
     * @brief 写入整数
     * @param rootKey 根键
     * @param key 子键
     * @param value 值
     */
    void setInt(const std::string& rootKey, const std::string& key, int value);

    /**
     * @brief 读取字符串数组（rootKey → key）
     * @param rootKey 根键
     * @param key 子键
     */
    std::vector<std::string> getStringArray(const std::string& rootKey, const std::string& key);

    /**
     * @brief 写入字符串数组
     * @param rootKey 根键
     * @param key 子键
     * @param values 值
     */
    void setStringArray(const std::string& rootKey, const std::string& key, const std::vector<std::string>& values);

    /**
     * @brief 查询根键是否存在
     * @param rootKey 根键
     */
    bool hasRootKey(const std::string& rootKey);

    /** @brief 获取所有根键 */
    std::vector<std::string> getRootKeys() const;

    /**
     * @brief 获取指定根键下的所有子键
     * @param rootKey 根键
     */
    std::vector<std::string> getKeys(const std::string& rootKey) const;

    /**
     * @brief 删除根键及其所有子键
     * @param rootKey 根键
     */
    void removeRootKey(const std::string& rootKey);

    /**
     * @brief 删除指定子键
     * @param rootKey 根键
     * @param key 子键
     */
    void removeKey(const std::string& rootKey, const std::string& key);

private:
    std::string m_path;
    yyjson_mut_doc* m_doc = nullptr;

    /** @brief 确保文档已初始化 */
    yyjson_mut_doc* ensureDoc();

    /**
     * @brief 查找 rootKey 对应的对象
     * @param rootKey 根键
     */
    void* findRootObj(const std::string& rootKey);

    /**
     * @brief 查找或创建 rootKey 对应的对象
     * @param rootKey 根键
     */
    void* findOrCreateRootObj(const std::string& rootKey);
};
