/**
 * PchtxtConverter - pchtxt 文本补丁转 IPS32 二进制
 */

#include "utils/pchtxtConverter.hpp"

#include <borealis/core/i18n.hpp>
#include <cstdlib>
#include <cstring>
#include <sstream>

namespace {

constexpr size_t NSOBID_MAX_LEN = 64;
constexpr size_t OFFSET_HEX_LEN = 8;
constexpr size_t PATCH_LINE_MIN_LEN = 11;

struct Patch {
    uint32_t offset;
    std::vector<uint8_t> data;
};

// ============================================================================
// 基础工具方法
// ============================================================================

bool isHexChar(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

uint8_t hexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    return 10 + (c - 'A');
}

const char* skipSpaces(const char* p) {
    while (*p == ' ' || *p == '\t') ++p;
    return p;
}

size_t tokenLen(const char* p) {
    size_t len = 0;
    while (p[len] && p[len] != ' ' && p[len] != '\t' &&
           p[len] != '/' && p[len] != '\r' && p[len] != '\n')
        ++len;
    return len;
}

// ============================================================================
// 大端写入
// ============================================================================

void writeBE32(std::vector<uint8_t>& buf, uint32_t val) {
    buf.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>(val & 0xFF));
}

void writeBE16(std::vector<uint8_t>& buf, uint16_t val) {
    buf.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>(val & 0xFF));
}

// ============================================================================
// 解析方法
// ============================================================================

std::string parseNsobid(const char* line) {
    if (strlen(line) <= 8 || line[0] != '@') return {};

    char prefix[8];
    memcpy(prefix, line + 1, 7);
    prefix[7] = '\0';
    for (int i = 0; i < 7; ++i)
        if (prefix[i] >= 'A' && prefix[i] <= 'Z')
            prefix[i] += 32;

    if (strcmp(prefix, "nsobid-") != 0) return {};

    const char* idStart = line + 8;
    size_t idLen = 0;
    while (idLen < NSOBID_MAX_LEN && isHexChar(idStart[idLen]))
        ++idLen;

    if (idLen == 0) return {};
    return std::string(idStart, idLen);
}

bool handleDirective(const char* cmd, bool& enabled, int32_t& offsetShift) {
    if (cmd[0] == 'e' || cmd[0] == 'E') {
        enabled = true;
        return false;
    }
    if (cmd[0] == 's' || cmd[0] == 'S') {
        return true;
    }
    if (cmd[0] == 'f' || cmd[0] == 'F') {
        if (strlen(cmd) < 4) return false;
        const char* flagBody = skipSpaces(cmd + 4);
        if (strncmp(flagBody, "offset_shift", 12) == 0) {
            const char* valStr = skipSpaces(flagBody + 12);
            offsetShift = static_cast<int32_t>(strtol(valStr, nullptr, 0));
        }
        return false;
    }
    enabled = false;
    return false;
}

std::string parseHexData(const char* p, size_t len, std::vector<uint8_t>& out) {
    if (len % 2 != 0) return brls::getStr("other/pchtxt/oddLength");
    out.reserve(len / 2);
    for (size_t i = 0; i < len; i += 2) {
        if (!isHexChar(p[i]) || !isHexChar(p[i + 1])) return brls::getStr("other/pchtxt/invalidHex");
        out.push_back((hexVal(p[i]) << 4) | hexVal(p[i + 1]));
    }
    return {};
}

std::string parseStringData(const char* p, std::vector<uint8_t>& out) {
    while (*p && *p != '"') {
        if (*p == '\\') {
            ++p;
            switch (*p) {
                case 'n':  out.push_back('\n'); break;
                case 't':  out.push_back('\t'); break;
                case 'r':  out.push_back('\r'); break;
                case '\\': out.push_back('\\'); break;
                case '"':  out.push_back('"');  break;
                case '0':  out.push_back('\0'); break;
                default:   return brls::getStr("other/pchtxt/unknownEscape");
            }
        } else {
            out.push_back(static_cast<uint8_t>(*p));
        }
        ++p;
    }
    if (*p != '"') return brls::getStr("other/pchtxt/unclosedString");
    out.push_back('\0');
    return {};
}

std::string parsePatchLine(const char* line, int32_t offsetShift, Patch& patch) {
    for (size_t i = 0; i < OFFSET_HEX_LEN; ++i)
        if (!isHexChar(line[i])) return brls::getStr("other/pchtxt/invalidOffset");

    char offsetBuf[9];
    memcpy(offsetBuf, line, 8);
    offsetBuf[8] = '\0';
    patch.offset = static_cast<uint32_t>(strtoul(offsetBuf, nullptr, 16));
    patch.offset += static_cast<uint32_t>(offsetShift);

    const char* dataStart = skipSpaces(line + OFFSET_HEX_LEN);
    if (*dataStart == '\0') return brls::getStr("other/pchtxt/missingData");

    if (*dataStart == '"')
        return parseStringData(dataStart + 1, patch.data);

    return parseHexData(dataStart, tokenLen(dataStart), patch.data);
}

// ============================================================================
// IPS32 生成
// ============================================================================

std::vector<uint8_t> generateIPS32(const std::vector<Patch>& patches) {
    size_t totalSize = 5 + 4;
    for (auto& p : patches)
        totalSize += 4 + 2 + p.data.size();

    std::vector<uint8_t> buf;
    buf.reserve(totalSize);

    const char* header = "IPS32";
    buf.insert(buf.end(), header, header + 5);

    for (auto& p : patches) {
        writeBE32(buf, p.offset);
        writeBE16(buf, static_cast<uint16_t>(p.data.size()));
        buf.insert(buf.end(), p.data.begin(), p.data.end());
    }

    const char* footer = "EEOF";
    buf.insert(buf.end(), footer, footer + 4);

    return buf;
}

} // namespace

// ============================================================================
// 公开接口
// ============================================================================

PchtxtConverter::Result PchtxtConverter::convert(const void* data, size_t size) {
    Result result{};

    if (!data || size == 0) {
        result.errorMsg = brls::getStr("other/pchtxt/emptyData");
        return result;
    }

    std::istringstream stream(std::string(static_cast<const char*>(data), size));
    std::string line;
    int lineNum = 0;

    auto readLine = [&]() -> bool {
        if (!std::getline(stream, line)) return false;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        return true;
    };

    if (!readLine()) {
        result.errorMsg = brls::getStr("other/pchtxt/emptyData");
        return result;
    }
    ++lineNum;

    if (line[0] == '@' && (line[1] == 'b' || line[1] == 'B' ||
                           line[1] == 'l' || line[1] == 'L')) {
        if (!readLine()) {
            result.errorMsg = brls::getStr("other/pchtxt/missingNsobid");
            return result;
        }
        ++lineNum;
    }

    result.nsobid = parseNsobid(line.c_str());
    if (result.nsobid.empty()) {
        result.errorMsg = brls::getStr("other/pchtxt/missingNsobid");
        return result;
    }

    bool enabled = false;
    int32_t offsetShift = 0;
    std::vector<Patch> patches;

    while (readLine()) {
        ++lineNum;

        if (line.empty() || line[0] == '#' || line[0] == '/')
            continue;

        if (line[0] == '@') {
            if (handleDirective(line.c_str() + 1, enabled, offsetShift)) break;
            continue;
        }

        if (!enabled || line.size() < PATCH_LINE_MIN_LEN)
            continue;

        Patch patch;
        std::string err = parsePatchLine(line.c_str(), offsetShift, patch);
        if (!err.empty()) {
            result.errorMsg = brls::getStr("other/pchtxt/lineError", std::to_string(lineNum), err);
            return result;
        }

        patches.push_back(std::move(patch));
    }

    if (patches.empty()) {
        result.errorMsg = brls::getStr("other/pchtxt/noPatchFound");
        return result;
    }

    result.ipsData = generateIPS32(patches);
    result.success = true;
    return result;
}
