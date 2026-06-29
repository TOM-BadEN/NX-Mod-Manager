/**
 * SearchEngine - 通用搜索引擎实现
 * 支持原文匹配和拼音混合匹配（全拼/首字母可任意组合）
 */

#include "utils/searchEngine.hpp"
#include "utils/pinYinCvt.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>

std::vector<SearchEngine::Result> SearchEngine::search(
    const std::string& keyword,
    const std::vector<std::string>& items,
    int maxResults)
{
    std::vector<Result> results;

    if (keyword.empty()) return results;

    // keyword 统一转大写（用于原文匹配）
    std::string upperKeyword = keyword;
    std::transform(upperKeyword.begin(), upperKeyword.end(), upperKeyword.begin(),
        [](unsigned char ch) { return std::toupper(ch); });

    // keyword 转拼音后清洗（用于混合拼音匹配，支持中文+拼音混合输入）
    std::string pinyinKeyword;
    {
        std::string rawPinyin = pinYinCvt::toPinyin(keyword);
        for (char ch : rawPinyin) {
            if (std::isalnum(static_cast<unsigned char>(ch))) {
                pinyinKeyword += static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
            }
        }
    }

    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        const std::string& name = items[i];
        if (name.empty()) continue;

        // 原文匹配：名称转大写后子串查找
        std::string upperName = name;
        std::transform(upperName.begin(), upperName.end(), upperName.begin(),
            [](unsigned char ch) { return std::toupper(ch); });
        if (upperName.find(upperKeyword) != std::string::npos) {
            results.push_back({i, name});
            if (static_cast<int>(results.size()) >= maxResults) break;
            continue;
        }

        // 拼音混合匹配（按需计算并缓存）
        if (pinyinKeyword.empty()) continue;
        const PinyinCache& cache = getPinyinCache(name);
        const auto& tokens = cache.tokens;

        // 从每个 token 位置尝试混合匹配（支持子串语义）
        bool matched = false;
        for (size_t startToken = 0; startToken < tokens.size(); ++startToken) {
            if (mixedMatch(pinyinKeyword, tokens, 0, startToken)) {
                matched = true;
                break;
            }
        }

        if (matched) {
            results.push_back({i, name});
            if (static_cast<int>(results.size()) >= maxResults) break;
        }
    }

    return results;
}

const SearchEngine::PinyinCache& SearchEngine::getPinyinCache(const std::string& name) {
    auto it = m_cache.find(name);
    if (it != m_cache.end()) return it->second;

    // toPinyin 返回空格分隔的拼音字符串（如 "sai er da Zelda"）
    std::string pinyin = pinYinCvt::toPinyin(name);

    PinyinCache entry;

    // 按空格分词，每个 token 仅保留字母和数字后转大写
    std::istringstream stream(pinyin);
    std::string rawToken;
    while (stream >> rawToken) {
        std::string cleanToken;
        for (char ch : rawToken) {
            if (std::isalnum(static_cast<unsigned char>(ch))) {
                cleanToken += static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
            }
        }
        if (!cleanToken.empty()) {
            entry.tokens.push_back(std::move(cleanToken));
        }
    }

    auto [inserted, success] = m_cache.emplace(name, std::move(entry));
    return inserted->second;
}

bool SearchEngine::mixedMatch(
    const std::string& keyword,
    const std::vector<std::string>& tokens,
    size_t keywordPos,
    size_t tokenPos)
{
    // keyword 已完全消费 → 匹配成功
    if (keywordPos >= keyword.size()) return true;

    // tokens 耗尽但 keyword 未消费完 → 匹配失败
    if (tokenPos >= tokens.size()) return false;

    const std::string& token = tokens[tokenPos];

    // 尝试首字母匹配：keyword 当前字符 == token 首字母
    if (keyword[keywordPos] == token[0]) {
        if (mixedMatch(keyword, tokens, keywordPos + 1, tokenPos + 1))
            return true;
    }

    // 尝试全拼匹配：keyword 从当前位置以 token 全拼开头
    size_t remaining = keyword.size() - keywordPos;
    if (remaining >= token.size()) {
        bool prefixMatch = true;
        for (size_t j = 0; j < token.size(); ++j) {
            if (keyword[keywordPos + j] != token[j]) {
                prefixMatch = false;
                break;
            }
        }
        if (prefixMatch) {
            if (mixedMatch(keyword, tokens, keywordPos + token.size(), tokenPos + 1))
                return true;
        }
    }

    return false;
}
