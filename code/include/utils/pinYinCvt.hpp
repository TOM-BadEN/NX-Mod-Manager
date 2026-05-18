/**
 * pinYinCvt - 通用拼音工具
 * 封装 cpp-pinyin 库，提供中文转拼音、排序键生成等功能
 */

#pragma once
#include <string>

namespace pinYinCvt {

    /** @brief 初始化拼音引擎（加载字典），应用启动时调用一次 */
    void init();

    /**
     * @brief 中文转拼音（无声调），非中文字符原样保留
     * @param text 输入文本
     */
    std::string toPinyin(const std::string& text);

    /**
     * @brief 获取排序键（全文拼音，大写）
     * @param text 输入文本
     */
    std::string getSortKey(const std::string& text);

}
