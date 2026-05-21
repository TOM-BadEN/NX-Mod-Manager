/**
 * StoreModManager - 商店模组列表数据管理
 * 持有商店模组列表、分页状态和筛选条件，纯数据层，不涉及 UI
 */

#pragma once

#include <vector>
#include <string>
#include <stop_token>
#include "api/mod.hpp"

class StoreModManager {

public:
    /**
     * @brief 构造商店模组列表管理器
     * @param gameTid 游戏 TID
     */
    StoreModManager(std::string gameTid);

    /**
     * @brief 获取下一页数据（子线程调用，读取内部筛选条件）
     * @param token 取消令牌
     */
    api::mod::ModListResult fetchNextPage(std::stop_token token = {});

    /**
     * @brief 追加一页数据（主线程调用）
     * @param result 数据结果
     */
    void appendPage(api::mod::ModListResult result);

    /** @brief 重置列表和分页（筛选条件变化时调用） */
    void reset();

    /** @brief 重置所有筛选条件为默认值 */
    void resetFilter();

    /** @brief 获取模组列表引用 */
    std::vector<api::mod::ModList>& storeModList();

    /** @brief 获取当前页码 */
    int currentPage() const { return m_currentPage; }

    /** @brief 获取总数 */
    int total() const;

    /** @brief 是否还有更多数据 */
    bool hasMore() const;

    /** @brief 获取游戏 TID */
    const std::string& gameTid() const;

    /**
     * @brief 获取模组适配的游戏版本列表（子线程调用）
     * @param token 取消令牌
     */
    api::mod::ModGameVersionsResult fetchGameVersions(std::stop_token token = {});

    /** @brief 获取排序方式 */
    const std::string& getSort() const { return m_sort; }

    /** @brief 获取搜索关键词 */
    const std::string& getKeyword() const { return m_keyword; }

    /** @brief 获取版本筛选 */
    const std::string& getVersion() const { return m_version; }

    /** @brief 获取模组类型筛选 */
    const std::string& getModType() const { return m_modType; }

    /**
     * @brief 设置排序方式
     * @param sort 排序字符串
     */
    void setSort(const std::string& sort);

    /**
     * @brief 设置搜索关键词
     * @param keyword 关键词
     */
    void setKeyword(const std::string& keyword);

    /**
     * @brief 设置版本筛选
     * @param version 版本字符串
     */
    void setVersion(const std::string& version);

    /**
     * @brief 设置模组类型筛选
     * @param modType 类型字符串
     */
    void setModType(const std::string& modType);

private:
    std::string m_gameTid;
    std::vector<api::mod::ModList> m_storeModList;
    int m_currentPage = 0;
    int m_total = 0;

    // 筛选条件（默认值与 Web 前端一致）
    std::string m_sort = "latest";
    std::string m_keyword;
    std::string m_version;
    std::string m_modType;
};
