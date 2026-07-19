/**
 * textClean - 文本清洗工具
 * 纯 header，inline，用于将 UTF-8 字符串转为文件系统安全的 ASCII 目录名
 */

#pragma once

#include <string>
#include <cstdint>
#include <vector>
#include <algorithm>

namespace textClean {

namespace {

    struct CharMap { 
        char32_t cp; 
        const char* to; 
    };

    // 拉丁变音 / 特殊符号 → ASCII 映射表（无序，由 getSortedMap() 排序后使用）
    static constexpr CharMap kRawMap[] = {
        {L'Á', "A"}, {L'À', "A"}, {L'Â', "A"}, {L'Ä', "A"}, {L'Ã', "A"}, {L'Å', "A"},
        {L'Ā', "A"}, {L'Ă', "A"}, {L'Ą', "A"}, {L'Ǎ', "A"}, {L'Ǻ', "A"}, {L'Ȁ', "A"},
        {L'Ȃ', "A"}, {L'Ǟ', "A"}, {L'Ǡ', "A"}, {L'á', "a"}, {L'à', "a"}, {L'â', "a"},
        {L'ä', "a"}, {L'ã', "a"}, {L'å', "a"}, {L'ā', "a"}, {L'ă', "a"}, {L'ą', "a"},
        {L'ǎ', "a"}, {L'ǻ', "a"}, {L'ȁ', "a"}, {L'ȃ', "a"}, {L'ǟ', "a"}, {L'ǡ', "a"},
        {L'Æ', "AE"}, {L'æ', "ae"}, {L'Ǣ', "AE"}, {L'ǣ', "ae"}, {L'Ɓ', "B"}, {L'ƀ', "b"},
        {L'Ƃ', "B"}, {L'ƃ', "b"}, {L'Ç', "C"}, {L'ç', "c"}, {L'Ć', "C"}, {L'ć', "c"},
        {L'Ĉ', "C"}, {L'ĉ', "c"}, {L'Ċ', "C"}, {L'ċ', "c"}, {L'Č', "C"}, {L'č', "c"},
        {L'Ƈ', "C"}, {L'ƈ', "c"}, {L'Ď', "D"}, {L'ď', "d"}, {L'Đ', "D"}, {L'đ', "d"},
        {L'Ɖ', "D"}, {L'ƌ', "d"}, {L'Ḑ', "D"}, {L'ḑ', "d"}, {L'Ḓ', "D"}, {L'ḓ', "d"},
        {L'É', "E"}, {L'È', "E"}, {L'Ê', "E"}, {L'Ë', "E"}, {L'Ē', "E"}, {L'Ĕ', "E"},
        {L'Ė', "E"}, {L'Ę', "E"}, {L'Ě', "E"}, {L'Ȅ', "E"}, {L'Ȇ', "E"}, {L'é', "e"},
        {L'è', "e"}, {L'ê', "e"}, {L'ë', "e"}, {L'ē', "e"}, {L'ĕ', "e"}, {L'ė', "e"},
        {L'ę', "e"}, {L'ě', "e"}, {L'ȅ', "e"}, {L'ȇ', "e"}, {L'Ĝ', "G"}, {L'Ğ', "G"},
        {L'Ġ', "G"}, {L'Ģ', "G"}, {L'Ǵ', "G"}, {L'Ǧ', "G"}, {L'Ǥ', "G"}, {L'ĝ', "g"},
        {L'ğ', "g"}, {L'ġ', "g"}, {L'ģ', "g"}, {L'ǵ', "g"}, {L'ǧ', "g"}, {L'ǥ', "g"},
        {L'Ĥ', "H"}, {L'Ħ', "H"}, {L'Ȟ', "H"}, {L'Ḩ', "H"}, {L'ḩ', "h"}, {L'ĥ', "h"},
        {L'ħ', "h"}, {L'ȟ', "h"}, {L'Í', "I"}, {L'Ì', "I"}, {L'Î', "I"}, {L'Ï', "I"},
        {L'Ī', "I"}, {L'Ĭ', "I"}, {L'Į', "I"}, {L'İ', "I"}, {L'Ǐ', "I"}, {L'Ȉ', "I"},
        {L'Ȋ', "I"}, {L'í', "i"}, {L'ì', "i"}, {L'î', "i"}, {L'ï', "i"}, {L'ī', "i"},
        {L'ĭ', "i"}, {L'į', "i"}, {L'ı', "i"}, {L'ǐ', "i"}, {L'ȉ', "i"}, {L'ȋ', "i"},
        {L'Ĵ', "J"}, {L'ĵ', "j"}, {L'Ķ', "K"}, {L'ķ', "k"}, {L'Ǩ', "K"}, {L'ǩ', "k"},
        {L'Ĺ', "L"}, {L'Ļ', "L"}, {L'Ľ', "L"}, {L'Ŀ', "L"}, {L'Ł', "L"}, {L'ĺ', "l"},
        {L'ļ', "l"}, {L'ľ', "l"}, {L'ŀ', "l"}, {L'ł', "l"}, {L'Ñ', "N"}, {L'Ń', "N"},
        {L'Ņ', "N"}, {L'Ň', "N"}, {L'Ǹ', "N"}, {L'ñ', "n"}, {L'ń', "n"}, {L'ņ', "n"},
        {L'ň', "n"}, {L'ǹ', "n"}, {L'Ó', "O"}, {L'Ò', "O"}, {L'Ô', "O"}, {L'Ö', "O"},
        {L'Õ', "O"}, {L'Ø', "O"}, {L'Ō', "O"}, {L'Ŏ', "O"}, {L'Ő', "O"}, {L'Ǒ', "O"},
        {L'Ȍ', "O"}, {L'Ȏ', "O"}, {L'Ǫ', "O"}, {L'Ǭ', "O"}, {L'ó', "o"}, {L'ò', "o"},
        {L'ô', "o"}, {L'ö', "o"}, {L'õ', "o"}, {L'ø', "o"}, {L'ō', "o"}, {L'ŏ', "o"},
        {L'ő', "o"}, {L'ǒ', "o"}, {L'ȍ', "o"}, {L'ȏ', "o"}, {L'ǫ', "o"}, {L'ǭ', "o"},
        {L'Œ', "OE"}, {L'œ', "oe"}, {L'Ŕ', "R"}, {L'Ŗ', "R"}, {L'Ř', "R"}, {L'ŕ', "r"},
        {L'ŗ', "r"}, {L'ř', "r"}, {L'Ś', "S"}, {L'Ŝ', "S"}, {L'Ş', "S"}, {L'Š', "S"},
        {L'ś', "s"}, {L'ŝ', "s"}, {L'ş', "s"}, {L'š', "s"}, {L'ẞ', "Ss"}, {L'ß', "ss"},
        {L'Ţ', "T"}, {L'Ť', "T"}, {L'Ŧ', "T"}, {L'ţ', "t"}, {L'ť', "t"}, {L'ŧ', "t"},
        {L'Ú', "U"}, {L'Ù', "U"}, {L'Û', "U"}, {L'Ü', "U"}, {L'Ū', "U"}, {L'Ŭ', "U"},
        {L'Ů', "U"}, {L'Ű', "U"}, {L'Ų', "U"}, {L'Ǔ', "U"}, {L'Ǖ', "U"}, {L'Ǘ', "U"},
        {L'Ǚ', "U"}, {L'Ǜ', "U"}, {L'Ȕ', "U"}, {L'Ȗ', "U"}, {L'ú', "u"}, {L'ù', "u"},
        {L'û', "u"}, {L'ü', "u"}, {L'ū', "u"}, {L'ŭ', "u"}, {L'ů', "u"}, {L'ű', "u"},
        {L'ų', "u"}, {L'ǔ', "u"}, {L'ǖ', "u"}, {L'ǘ', "u"}, {L'ǚ', "u"}, {L'ǜ', "u"},
        {L'ȕ', "u"}, {L'ȗ', "u"}, {L'Ŵ', "W"}, {L'Ẁ', "W"}, {L'Ẃ', "W"}, {L'Ẅ', "W"},
        {L'ŵ', "w"}, {L'ẁ', "w"}, {L'ẃ', "w"}, {L'ẅ', "w"}, {L'Ý', "Y"}, {L'Ÿ', "Y"},
        {L'Ŷ', "Y"}, {L'Ȳ', "Y"}, {L'ý', "y"}, {L'ÿ', "y"}, {L'ŷ', "y"}, {L'ȳ', "y"},
        {L'Ź', "Z"}, {L'Ż', "Z"}, {L'Ž', "Z"}, {L'ź', "z"}, {L'ż', "z"}, {L'ž', "z"},
        {L'‐', "-"}, {L'–', "-"}, {L'—', "-"}, {L'―', "-"}, {L'‘', "'"}, {L'’', "'"},
        {L'‛', "'"}, {L'′', "'"}, {L'ʼ', "'"}, {L'“', "\""}, {L'”', "\""}, {L'„', "\""},
        {L'″', "\""}, {L'※', "*"}, {L'×', "x"}, {L' ', " "}, {L' ', " "}, {L' ', " "},
        {L'Ⅰ', "I"}, {L'Ⅱ', "II"}, {L'Ⅲ', "III"}, {L'Ⅳ', "IV"}, {L'Ⅴ', "V"}, {L'Ⅵ', "VI"},
        {L'Ⅶ', "VII"}, {L'Ⅷ', "VIII"}, {L'Ⅸ', "IX"}, {L'Ⅹ', "X"}
    };

    /** @brief 获取按 codepoint 排序的映射表（首次调用时排序，之后直接返回） */
    inline const std::vector<CharMap>& getSortedMap() {
        static const auto sorted = [] {
            std::vector<CharMap> v(std::begin(kRawMap), std::end(kRawMap));
            std::sort(v.begin(), v.end(), [](const CharMap& a, const CharMap& b) { return a.cp < b.cp; });
            return v;
        }();
        return sorted;
    }

    /** @brief 在排序映射表中查找 codepoint，命中返回映射字符串指针，未命中返回 nullptr */
    inline const char* lookupMap(char32_t cp) {
        const auto& m = getSortedMap();
        auto it = std::lower_bound(m.begin(), m.end(), cp, [](const CharMap& e, char32_t c) { return e.cp < c; });
        if (it != m.end() && it->cp == cp) return it->to;
        return nullptr;
    }

    /** @brief 是否为文件系统禁用字符 */
    inline bool isForbiddenChar(char32_t c) {
        static constexpr char32_t kForbidden[] = {
            L',', L'/', L'\\', L'<', L'>', L':', L'"', L'|', L'?', L'*', L'™', L'©', L'®'
        };
        for (auto f : kForbidden) if (c == f) return true;
        return false;
    }

    /**
     * @brief 从 UTF-8 字节流中解码一个 codepoint
     * @param s 字节流
     * @param len 字节流总长度
     * @param pos 当前偏移
     * @param[out] out 解码出的 codepoint
     * @return 消耗的字节数，失败返回 0
     */
    inline size_t decodeUtf8(const char* s, size_t len, size_t pos, char32_t& out) {
        if (pos >= len) return 0;
        auto b = static_cast<uint8_t>(s[pos]);
        if (b < 0x80) { out = b; return 1; }
        size_t n; char32_t cp;
        if ((b & 0xE0) == 0xC0)      { n = 2; cp = b & 0x1F; }
        else if ((b & 0xF0) == 0xE0)  { n = 3; cp = b & 0x0F; }
        else if ((b & 0xF8) == 0xF0)  { n = 4; cp = b & 0x07; }
        else return 0;
        if (pos + n > len) return 0;
        for (size_t i = 1; i < n; i++) {
            auto c = static_cast<uint8_t>(s[pos + i]);
            if ((c & 0xC0) != 0x80) return 0;
            cp = (cp << 6) | (c & 0x3F);
        }
        static constexpr char32_t kMinCp[] = {0, 0, 0x80, 0x800, 0x10000};
        if (cp < kMinCp[n]) return 0;
        out = cp;
        return n;
    }

} // anonymous namespace


/**
 * @brief 字符串 → 文件系统安全的目录名
 *
 * 处理逻辑（逐 UTF-8 codepoint）：
 *   1. 查映射表 → 命中 → 追加映射结果（如 é→e, Æ→AE）
 *   2. 禁用字符（/ \ : * ? " < > | , ™ © ® 等）→ 跳过
 *   3. 非 ASCII（0x20-0x7E 之外）→ return ""（整个放弃，fallback TID）
 *   4. 正常 ASCII → 追加原字符
 *
 * @param name 原始字符串（UTF-8）
 * @return 安全目录名，空串表示无法转换（调用方应 fallback 到 TID）
 */
inline std::string safeDirName(const std::string& name) {
    std::string result;
    result.reserve(name.size());
    const char* s = name.c_str();
    size_t len = name.size();

    for (size_t i = 0; i < len;) {
        char32_t cp;
        size_t n = decodeUtf8(s, len, i, cp);
        if (n == 0) return "";

        // 1. 查映射表
        const char* mapped = lookupMap(cp);
        if (mapped) {
            result += mapped;
            i += n;
            continue;
        }

        // 2. 禁用字符 → 跳过
        if (isForbiddenChar(cp)) {
            i += n;
            continue;
        }

        // 3. ASCII 范围检查（0x20-0x7E）
        if (cp < 0x20 || cp > 0x7E) return "";

        // 4. 正常 ASCII → 追加
        result += static_cast<char>(cp);
        i += n;
    }

    while (!result.empty() && (result.back() == ' ' || result.back() == '.')) {
        result.pop_back();
    }
    return result;
}

/**
 * @brief 判断相对路径是否可安全用于文件系统
 *
 * 不修改原路径，仅允许可打印 ASCII 字符和作为路径分隔符的 `/`。
 * 任意一级名称包含路径禁用字符或非 ASCII 字符时返回 false。
 *
 * @param path 以 `\0` 结尾的相对路径
 * @return 路径安全返回 true，否则返回 false
 */
inline bool isSafeRelativePath(const char* path) {
    if (!path || path[0] == '\0' || path[0] == '/') return false;

    bool segmentEmpty = true;

    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(path); *p != '\0'; ++p) {
        unsigned char c = *p;

        if (c == '/') {
            if (segmentEmpty) return false;
            if (p[1] == '\0') return true;
            segmentEmpty = true;
            continue;
        }

        if (c < 0x20 || c > 0x7E) return false;

        switch (c) {
            case '\\':
            case '<':
            case '>':
            case ':':
            case '"':
            case '|':
            case '?':
            case '*':
                return false;
            default:
                break;
        }

        segmentEmpty = false;
    }

    return !segmentEmpty;
}

} // namespace textClean
