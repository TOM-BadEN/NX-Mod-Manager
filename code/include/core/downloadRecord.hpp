/**
 * DownloadRecord - 商店下载记录
 * 追踪从商店下载安装的 mod，用于检测重复安装
 * 数据文件：纯文本，每行一个 modId
 */

#pragma once

#include <string>
#include <unordered_set>

class DownloadRecord {
public:
    /** @brief 从磁盘加载到内存 */
    void load();

    /** @brief 将内存写回磁盘 */
    void save();

    /**
     * @brief 是否已记录
     * @param modId 模组 ID
     */
    bool has(int modId) const;

    /**
     * @brief 添加记录
     * @param modId 模组 ID
     */
    void add(int modId);

    /**
     * @brief 删除记录
     * @param modId 模组 ID
     */
    void remove(int modId);

private:
    std::unordered_set<int> m_ids;
};
