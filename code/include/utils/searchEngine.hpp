/**
 * SearchEngine - 通用搜索引擎
 * 支持原文匹配和拼音混合匹配（全拼/首字母可任意组合）
 * 拼音结果按需计算并缓存（memoization）
 */

#pragma once
#include <string>
#include <vector>
#include <unordered_map>

class SearchEngine {
public:
    struct Result {
        int index;            // 原始列表中的下标
        std::string name;     // 匹配到的名称
    };

    /**
     * @brief 搜索：对 items 做匹配，返回最多 maxResults 条结果
     * @param keyword 搜索关键词
     * @param items 待搜索列表
     * @param maxResults 最大返回数
     */
    std::vector<Result> search(const std::string& keyword,
                               const std::vector<std::string>& items,
                               int maxResults = 6);

private:
    struct PinyinCache {
        std::vector<std::string> tokens;  // 每个拼音 token，全大写（如 ["BAO", "KE", "MENG"]）
    };

    std::unordered_map<std::string, PinyinCache> m_cache; // memoization 缓存

    /**
     * @brief 按需计算并缓存拼音
     * @param name 原始名称
     */
    const PinyinCache& getPinyinCache(const std::string& name);

    /**
     * @brief 混合匹配：递归尝试全拼或首字母消费
     * @param keyword 搜索关键词
     * @param tokens 拼音 token 列表
     * @param keywordPos keyword 当前位置
     * @param tokenPos tokens 当前位置
     */
    static bool mixedMatch(const std::string& keyword, const std::vector<std::string>& tokens,
                           size_t keywordPos, size_t tokenPos);
};
