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

    // 拼音排序
    // getName: 成员指针或 lambda，从元素提取排序用的字符串
    // ascending: true=A-Z, false=Z-A
    // 示例: strSort::sortAZ(games, &GameInfo::displayName);
    template<typename T, typename GetName>
    void sortAZ(std::vector<T>& items, GetName getName, bool ascending = true) {
        std::unordered_map<std::string, std::string> cache;
        for (const auto& item : items) {
            const auto& name = std::invoke(getName, item);
            cache.emplace(name, pinYinCvt::getSortKey(name));
        }
        std::sort(items.begin(), items.end(), [&](const T& a, const T& b) {
            return ascending
                ? cache[std::invoke(getName, a)] < cache[std::invoke(getName, b)]
                : cache[std::invoke(getName, a)] > cache[std::invoke(getName, b)];
        });
    }

    // 分组 + 拼音排序
    // groupBy 为 true 的排在前面，组内按拼音排序
    // ascending: true=A-Z, false=Z-A
    // 示例: strSort::sortAZ(games, &GameInfo::displayName, &GameInfo::isFavorite);
    template<typename T, typename GetName, typename GroupBy>
    void sortAZ(std::vector<T>& items, GetName getName, GroupBy groupBy, bool ascending = true) {
        std::unordered_map<std::string, std::string> cache;
        for (const auto& item : items) {
            const auto& name = std::invoke(getName, item);
            cache.emplace(name, pinYinCvt::getSortKey(name));
        }
        std::sort(items.begin(), items.end(), [&](const T& a, const T& b) {
            auto ga = std::invoke(groupBy, a);
            auto gb = std::invoke(groupBy, b);
            if (ga != gb) return ga > gb;
            return ascending
                ? cache[std::invoke(getName, a)] < cache[std::invoke(getName, b)]
                : cache[std::invoke(getName, a)] > cache[std::invoke(getName, b)];
        });
    }

    // 三级排序：bool 分组 + 可比较分组 + 拼音
    // group1 为 true 的排在前面，group2 相同的聚集，组内按拼音排序
    // 示例: strSort::sortAZ(mods, &ModInfo::displayName, &ModInfo::isInstalled, &ModInfo::type);
    template<typename T, typename GetName, typename Group1, typename Group2,
             typename = std::enable_if_t<!std::is_same_v<std::decay_t<Group2>, bool>>>
    void sortAZ(std::vector<T>& items, GetName getName, Group1 group1, Group2 group2,
                bool ascending = true) {
        std::unordered_map<std::string, std::string> cache;
        for (const auto& item : items) {
            const auto& name = std::invoke(getName, item);
            cache.emplace(name, pinYinCvt::getSortKey(name));
        }
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
