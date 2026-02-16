/**
 * pinyinHelper - 通用拼音工具实现
 */

#include "utils/pinyinHelper.hpp"
#include <cpp-pinyin/Pinyin.h>
#include <cpp-pinyin/G2pglobal.h>
#include <memory>
#include <algorithm>
#include <cctype>

namespace pinyinHelper {

static std::unique_ptr<Pinyin::Pinyin> s_pinyin;

void init() {
    if (s_pinyin) return;
    Pinyin::setDictionaryPath("romfs:/dict/mandarin");
    s_pinyin = std::make_unique<Pinyin::Pinyin>();
}

std::string toPinyin(const std::string& text) {
    if (!s_pinyin || text.empty()) return text;
    auto res = s_pinyin->hanziToPinyin(text, Pinyin::ManTone::Style::NORMAL);
    return res.toStdStr();
}

std::string getSortKey(const std::string& text) {
    if (text.empty()) return "";
    if (!s_pinyin) return text.substr(0, 1);

    // 取首字符，让 cpp-pinyin 处理
    // UTF-8 首字符可能是 1~4 字节
    unsigned char c = text[0];
    int len = 1;
    if (c >= 0xF0 && text.size() >= 4) len = 4;
    else if (c >= 0xE0 && text.size() >= 3) len = 3;
    else if (c >= 0xC0 && text.size() >= 2) len = 2;

    std::string firstChar = text.substr(0, len);

    // 用 cpp-pinyin 转换首字符
    auto res = s_pinyin->hanziToPinyin(firstChar, Pinyin::ManTone::Style::NORMAL);
    std::string key = res.empty() ? firstChar : res[0].pinyin;

    // 转大写
    std::transform(key.begin(), key.end(), key.begin(),
        [](unsigned char ch) { return std::toupper(ch); });

    return key;
}

}
