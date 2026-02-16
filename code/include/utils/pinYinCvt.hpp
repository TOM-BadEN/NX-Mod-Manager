/**
 * pinYinCvt - 通用拼音工具
 * 封装 cpp-pinyin 库，提供中文转拼音、排序键生成等功能
 */

#pragma once
#include <string>

namespace pinYinCvt {

    // 初始化拼音引擎（加载字典），应用启动时调用一次
    void init();

    // 中文转拼音（无声调），非中文字符原样保留
    // "塞尔达Zelda" → "sai er da Zelda"
    std::string toPinyin(const std::string& text);

    // 获取排序键（首字符拼音或原字符，大写）
    // "塞尔达" → "SAI"  "Zelda" → "Z"  "ゼルダ" → "ゼ"
    std::string getSortKey(const std::string& text);

}
