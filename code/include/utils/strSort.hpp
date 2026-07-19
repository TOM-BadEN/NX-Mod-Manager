/**
 * strSort - 字符串排序工具
 * 基于拼音的多语言排序（中文按拼音，英文直接，其他按 Unicode）
 * 内部自动预计算排序键，调用方无需关心缓存细节
 */

#pragma once
#include "utils/pinYinCvt.hpp"
#include <vector>
#include <string>
#include <algorithm>
#include <unordered_map>
#include <functional>

namespace strSort {

    /**
     * @brief 构建拼音缓存
     * @param items 待排序列表
     * @param getName 从元素提取排序字符串的成员指针或 lambda
     */
    template<typename T, typename GetName>
    std::unordered_map<std::string, std::string> buildCache(const std::vector<T>& items, GetName getName) {
        std::unordered_map<std::string, std::string> cache;
        for (const auto& item : items) {
            const auto& name = std::invoke(getName, item);
            cache.emplace(name, pinYinCvt::getSortKey(name));
        }
        return cache;
    }

    /**
     * @brief 用缓存比较两个名字
     * @param cache 拼音缓存
     * @param a 名字 a
     * @param b 名字 b
     */
    inline bool compareByCache(const std::unordered_map<std::string, std::string>& cache,
                               const std::string& a, const std::string& b) {
        return cache.at(a) < cache.at(b);
    }

    /**
     * @brief 拼音排序
     * @param items 待排序列表
     * @param getName 从元素提取排序字符串的成员指针或 lambda
     * @param ascending true=A-Z, false=Z-A
     */
    template<typename T, typename GetName>
    void sortAZ(std::vector<T>& items, GetName getName, bool ascending = true) {
        auto cache = buildCache(items, getName);
        std::sort(items.begin(), items.end(), [&](const T& a, const T& b) {
            return ascending
                ? cache[std::invoke(getName, a)] < cache[std::invoke(getName, b)]
                : cache[std::invoke(getName, a)] > cache[std::invoke(getName, b)];
        });
    }

    /**
     * @brief 分组 + 拼音排序（groupBy 为 true 的排在前面）
     * @param items 待排序列表
     * @param getName 从元素提取排序字符串
     * @param groupBy 分组条件
     * @param ascending true=A-Z, false=Z-A
     */
    template<typename T, typename GetName, typename GroupBy>
    void sortAZ(std::vector<T>& items, GetName getName, GroupBy groupBy, bool ascending = true) {
        auto cache = buildCache(items, getName);
        std::sort(items.begin(), items.end(), [&](const T& a, const T& b) {
            auto ga = std::invoke(groupBy, a);
            auto gb = std::invoke(groupBy, b);
            if (ga != gb) return ga > gb;
            return ascending
                ? cache[std::invoke(getName, a)] < cache[std::invoke(getName, b)]
                : cache[std::invoke(getName, a)] > cache[std::invoke(getName, b)];
        });
    }

    /**
     * @brief 三级排序：bool 分组 + 可比较分组 + 拼音
     * @param items 待排序列表
     * @param getName 从元素提取排序字符串
     * @param group1 一级分组条件（bool）
     * @param group2 二级分组条件（可比较）
     * @param ascending true=A-Z, false=Z-A
     */
    template<typename T, typename GetName, typename Group1, typename Group2,
             typename = std::enable_if_t<!std::is_same_v<std::decay_t<Group2>, bool>>>
    void sortAZ(std::vector<T>& items, GetName getName, Group1 group1, Group2 group2,
                bool ascending = true) {
        auto cache = buildCache(items, getName);
        std::sort(items.begin(), items.end(), [&](const T& a, const T& b) {
            auto ga = std::invoke(group1, a);
            auto gb = std::invoke(group1, b);
            if (ga != gb) return ga > gb;
            auto sa = std::invoke(group2, a);
            auto sb = std::invoke(group2, b);
            if (sa != sb) return sa < sb;
            return ascending
                ? cache[std::invoke(getName, a)] < cache[std::invoke(getName, b)]
                : cache[std::invoke(getName, a)] > cache[std::invoke(getName, b)];
        });
    }

}
