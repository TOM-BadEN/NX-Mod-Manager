/**
 * Settings - 全局运行时配置（单文件）
 * 启动时从 SD 卡 JSON 加载，set 时可自动持久化
 * 基于 JsonFile 封装，category 对应 JSON 的 rootKey
 */

#pragma once

#include <string>
#include "utils/jsonFile.hpp"
#include "common/config.hpp"

class Settings {
public:
    /** @brief 从 SD 卡加载配置（启动时调用一次） */
    static void load() {
        json().load(config::settingsPath);
    }

    /** @brief 持久化到 SD 卡 */
    static void save() {
        json().save();
    }

    /**
     * @brief 读取字符串配置
     * @param category 分类
     * @param key 键名
     * @param defaultValue 默认值
     */
    static std::string getString(const std::string& category, const std::string& key, const std::string& defaultValue = "") {
        return json().getString(category, key, defaultValue);
    }

    /**
     * @brief 读取布尔配置
     * @param category 分类
     * @param key 键名
     * @param defaultValue 默认值
     */
    static bool getBool(const std::string& category, const std::string& key, bool defaultValue = false) {
        return json().getBool(category, key, defaultValue);
    }

    /**
     * @brief 写入字符串配置
     * @param category 分类
     * @param key 键名
     * @param value 值
     * @param save 是否立即持久化
     */
    static void setString(const std::string& category, const std::string& key, const std::string& value, bool save = true) {
        json().setString(category, key, value);
        if (save) json().save();
    }

    /**
     * @brief 写入布尔配置
     * @param category 分类
     * @param key 键名
     * @param value 值
     * @param save 是否立即持久化
     */
    static void setBool(const std::string& category, const std::string& key, bool value, bool save = true) {
        json().setBool(category, key, value);
        if (save) json().save();
    }

    /**
     * @brief 查询键是否存在
     * @param category 分类
     * @param key 键名
     */
    static bool hasKey(const std::string& category, const std::string& key) {
        return !json().getString(category, key, "").empty();
    }

    /**
     * @brief 删除指定键
     * @param category 分类
     * @param key 键名
     */
    static void removeKey(const std::string& category, const std::string& key) {
        json().removeKey(category, key);
    }

private:
    static JsonFile& json() {
        static JsonFile instance;
        return instance;
    }
};
