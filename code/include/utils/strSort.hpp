/**
 * strSort - 字符串排序比较器
 * 基于拼音的多语言字符串比较（中文按拼音，英文直接，其他按 Unicode）
 */

#pragma once
#include "utils/pinYinCvt.hpp"

namespace strSort {

inline bool compareAZ(const std::string& a, const std::string& b) {
    return pinYinCvt::getSortKey(a) < pinYinCvt::getSortKey(b);
}

inline bool compareZA(const std::string& a, const std::string& b) {
    return pinYinCvt::getSortKey(a) > pinYinCvt::getSortKey(b);
}

}
