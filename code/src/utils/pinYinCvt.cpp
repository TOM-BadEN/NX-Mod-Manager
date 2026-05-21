/**
 * pinYinCvt - 通用拼音工具实现
 */

#include "utils/pinYinCvt.hpp"
#include <cpp-pinyin/Pinyin.h>
#include <cpp-pinyin/G2pglobal.h>
#include <memory>
#include <algorithm>
#include <cctype>

namespace pinYinCvt {

static std::unique_ptr<Pinyin::Pinyin> s_pinyin;

void init() {
    if (s_pinyin) return;
    Pinyin::setDictionaryPath("romfs:/dict");
    s_pinyin = std::make_unique<Pinyin::Pinyin>();
}

std::string toPinyin(const std::string& text) {
    if (!s_pinyin || text.empty()) return text;
    auto res = s_pinyin->hanziToPinyin(text, Pinyin::ManTone::Style::NORMAL);
    return res.toStdStr();
}

std::string getSortKey(const std::string& text) {
    if (text.empty()) return "";
    if (!s_pinyin) return text;

    // 全文转拼音（中文 → 拼音，英文原样保留），统一大写
    std::string key = toPinyin(text);
    std::transform(key.begin(), key.end(), key.begin(),
        [](unsigned char ch) { return std::toupper(ch); });
    return key;
}

}
