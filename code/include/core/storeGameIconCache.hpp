/**
 * StoreGameIconCache - 商店游戏图标本地缓存
 * 管理本地 WebP 文件和 HTTP 缓存元数据，不涉及网络、UI 和 NVG 纹理
 */

#pragma once

#include "utils/jsonFile.hpp"
#include <cstdint>
#include <stop_token>
#include <string>
#include <vector>

/** @brief 商店游戏图标本地缓存 */
class StoreGameIconCache {
public:
    /** @brief 单个游戏图标的 HTTP 缓存元数据 */
    struct Metadata {
        std::string etag;         // ETag
        std::string lastModified; // Last-Modified
    };

    /** @brief 创建商店游戏图标缓存 */
    StoreGameIconCache() = default;

    /**
     * @brief 加载元数据 JSON
     * @return 加载成功时返回 true，否则返回 false
     */
    bool load();

    /**
     * @brief 保存元数据 JSON
     * @return 保存成功时返回 true，否则返回 false
     */
    bool save();

    /**
     * @brief 获取图标 WebP 路径
     * @param tid 游戏 TID
     * @return 图标文件路径
     */
    static std::string iconPath(const std::string& tid);

    /**
     * @brief 读取本地图标 WebP 原始数据
     * @param tid 游戏 TID
     * @return 图标原始数据，读取失败时返回空数组
     */
    static std::vector<uint8_t> readIcon(const std::string& tid);

    /**
     * @brief 写入本地图标 WebP，失败时不破坏旧文件
     * @param tid 游戏 TID
     * @param data WebP 原始数据
     * @return 写入成功时返回 true，否则返回 false
     */
    static bool writeIcon(const std::string& tid, const std::vector<uint8_t>& data);

    /** @brief 删除本地图标文件缓存和元数据 */
    static void deleteCache();

    /**
     * @brief 获取本地图标文件缓存占用大小
     * @param token 取消令牌
     * @return 缓存占用字节数，取消时返回 -1
     */
    static int64_t cacheSize(std::stop_token token = {});

    /**
     * @brief 读取图标缓存元数据
     * @param tid 游戏 TID
     * @return 图标缓存元数据
     */
    Metadata metadata(const std::string& tid);

    /**
     * @brief 更新图标缓存元数据
     * @param tid 游戏 TID
     * @param etag 响应头中的 ETag
     * @param lastModified 响应头中的 Last-Modified
     */
    void updateMetadata(const std::string& tid, const std::string& etag, const std::string& lastModified);

private:
    JsonFile m_json; // HTTP 缓存元数据 JSON
    bool m_dirty = false; // 缓存元数据是否需要保存
};
