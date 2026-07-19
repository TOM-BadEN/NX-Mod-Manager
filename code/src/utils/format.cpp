/**
 * format - 通用格式化工具函数实现
 */

#include "utils/format.hpp"
#include <borealis/core/i18n.hpp>
#include <cstdio>
#include <cstdlib>
#include <ctime>

namespace {

int64_t civilDaysSinceUnixEpoch(int year, unsigned month, unsigned day) {
    year -= month <= 2;
    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(year - era * 400);
    const unsigned doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097LL + static_cast<int>(doe) - 719468;
}

bool parseIsoTimezoneOffset(const char* text, int& offsetSeconds) {
    if (!text || !*text) return false;

    if (*text == 'Z' || *text == 'z') {
        offsetSeconds = 0;
        return true;
    }

    if (*text != '+' && *text != '-') return false;

    int hour = 0;
    int minute = 0;
    if (std::sscanf(text + 1, "%2d:%2d", &hour, &minute) != 2) return false;
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59) return false;

    offsetSeconds = (hour * 3600 + minute * 60) * (*text == '-' ? -1 : 1);
    return true;
}

bool parseIsoTimestamp(const std::string& dateString, std::time_t& target) {
    int year, month, day, hour, minute, second;
    int parsedChars = 0;
    int parsed = std::sscanf(dateString.c_str(), "%d-%d-%d%*c%d:%d:%d%n",
        &year, &month, &day, &hour, &minute, &second, &parsedChars);
    if (parsed < 6) return false;

    if (month < 1 || month > 12 || day < 1 || day > 31 ||
        hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 60) {
        return false;
    }

    const char* timezone = dateString.c_str() + parsedChars;
    if (*timezone == '.') {
        timezone++;
        while (*timezone >= '0' && *timezone <= '9') timezone++;
    }

    int offsetSeconds = 0;
    if (!parseIsoTimezoneOffset(timezone, offsetSeconds)) return false;

    int64_t days = civilDaysSinceUnixEpoch(year, static_cast<unsigned>(month), static_cast<unsigned>(day));
    int64_t seconds = days * 86400LL + hour * 3600LL + minute * 60LL + second - offsetSeconds;
    target = static_cast<std::time_t>(seconds);
    return true;
}

} // namespace

namespace format {

std::string appIdHex(uint64_t appId) {
    char buf[17];
    std::snprintf(buf, sizeof(buf), "%016lX", appId);
    return std::string(buf);
}

uint64_t appIdFromHex(const std::string& hex) {
    char* end = nullptr;
    uint64_t val = std::strtoull(hex.c_str(), &end, 16);
    if (end == hex.c_str() || *end != '\0' || val == 0) return 0;
    return val;
}

bool isMainOrDlcTid(uint64_t mainTid, const std::string& tidHex) {
    uint64_t tid = appIdFromHex(tidHex);
    if (tid == mainTid) return true;
    if (tid == 0 || (tid & 0xFFF) == 0) return false;
    return ((tid ^ 0x1000) & ~0xFFFULL) == mainTid;
}

bool appIdIsValid(uint64_t appId) {
    return ((appId >> 48) == 0x0100) && ((appId & 0x1FFF) == 0);
}

std::string fileSize(int64_t bytes) {
    char buf[32];
    if (bytes < 1024) std::snprintf(buf, sizeof(buf), "%lld B", static_cast<long long>(bytes));
    else if (bytes < 1024 * 1024) std::snprintf(buf, sizeof(buf), "%.1f KB", bytes / 1024.0);
    else if (bytes < 1024LL * 1024 * 1024) std::snprintf(buf, sizeof(buf), "%.1f MB", bytes / (1024.0 * 1024));
    else std::snprintf(buf, sizeof(buf), "%.1f GB", bytes / (1024.0 * 1024 * 1024));
    return std::string(buf);
}

std::string elapsed(int64_t ms) {
    int totalSeconds = static_cast<int>(ms / 1000);
    if (totalSeconds < 60) {
        return brls::getStr("other/format/seconds", std::to_string(static_cast<int>(ms / 1000.0)));
    }
    int hours = totalSeconds / 3600;
    int minutes = (totalSeconds % 3600) / 60;
    int seconds = totalSeconds % 60;
    if (hours > 0) return brls::getStr("other/format/hms", std::to_string(hours), std::to_string(minutes), std::to_string(seconds));
    return brls::getStr("other/format/ms", std::to_string(minutes), std::to_string(seconds));
}

std::string resultHex(uint32_t result) {
    char buf[11];
    std::snprintf(buf, sizeof(buf), "0x%08X", result);
    return std::string(buf);
}

std::string transferSpeed(double bytesPerSecond) {
    char buf[32];
    if (bytesPerSecond < 1024) std::snprintf(buf, sizeof(buf), "%.0f B/s", bytesPerSecond);
    else if (bytesPerSecond < 1024 * 1024) std::snprintf(buf, sizeof(buf), "%.1f KB/s", bytesPerSecond / 1024.0);
    else if (bytesPerSecond < 1024.0 * 1024 * 1024) std::snprintf(buf, sizeof(buf), "%.1f MB/s", bytesPerSecond / (1024.0 * 1024));
    else std::snprintf(buf, sizeof(buf), "%.1f GB/s", bytesPerSecond / (1024.0 * 1024 * 1024));
    return std::string(buf);
}

std::string duration(int seconds, const std::string& format) {
    char buf[16];
    int h = seconds / 3600;
    int m = (seconds % 3600) / 60;
    int s = seconds % 60;
    if (format == "HH:MM:SS") std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", h, m, s);
    else if (format == "MM:SS") std::snprintf(buf, sizeof(buf), "%02d:%02d", m, s);
    else std::snprintf(buf, sizeof(buf), "%02d", s);
    return std::string(buf);
}

std::string timeAgo(const std::string& dateString) {
    if (dateString.empty()) return "";

    std::time_t target = 0;
    if (!parseIsoTimestamp(dateString, target)) {
        std::tm tm = {};
        int year, month, day, hour, minute, second;
        int parsed = std::sscanf(dateString.c_str(), "%d-%d-%d%*c%d:%d:%d", &year, &month, &day, &hour, &minute, &second);
        if (parsed < 6) return dateString;

        tm.tm_year = year - 1900;
        tm.tm_mon  = month - 1;
        tm.tm_mday = day;
        tm.tm_hour = hour;
        tm.tm_min  = minute;
        tm.tm_sec  = second;
        tm.tm_isdst = -1;

        target = std::mktime(&tm);
    }

    if (target == -1) return dateString;

    std::time_t now = std::time(nullptr);
    long long diff = static_cast<long long>(std::difftime(now, target));
    if (diff < 0) return dateString;

    if (diff < 60)        return brls::getStr("other/format/justNow");
    if (diff < 3600)      return brls::getStr("other/format/minutesAgo", std::to_string(diff / 60));
    if (diff < 86400)     return brls::getStr("other/format/hoursAgo", std::to_string(diff / 3600));
    if (diff < 2592000)   return brls::getStr("other/format/daysAgo", std::to_string(diff / 86400));
    if (diff < 31536000)  return brls::getStr("other/format/monthsAgo", std::to_string(diff / 2592000));
    return brls::getStr("other/format/yearsAgo", std::to_string(diff / 31536000));
}

std::string gameDirName(const std::string& dirPath) {
    auto pos2 = dirPath.rfind('/');
    if (pos2 != std::string::npos && pos2 > 0) {
        auto pos1 = dirPath.rfind('/', pos2 - 1);
        return dirPath.substr(pos1 + 1, pos2 - pos1 - 1);
    }
    return {};
}

long versionCode(const std::string& version) {
    const char* p = version.c_str();
    if (*p == 'v' || *p == 'V') p++;
    long code = 0;
    int seg = 0;
    while (*p && seg < 4) {
        int num = 0;
        while (*p >= '0' && *p <= '9') { num = num * 10 + (*p - '0'); p++; }
        code = code * 1000 + num;
        seg++;
        if (*p == '.') p++;
    }
    while (seg < 4) { code *= 1000; seg++; }
    return code;
}

std::string addBlankLineAfterLineBreaks(const std::string& text) {
    std::string result;
    result.reserve(text.size() * 2);

    for (size_t i = 0; i < text.size(); i++) {
        result.push_back(text[i]);
        if (text[i] != '\n') continue;

        bool prevIsLineBreak = i > 0 && text[i - 1] == '\n';
        bool nextIsLineBreak = i + 1 < text.size() && text[i + 1] == '\n';
        if (!prevIsLineBreak && !nextIsLineBreak) result.push_back('\n');
    }

    return result;
}

std::string cleanVersion(const std::string& version) {
    if (version.empty()) return version;
    if (version[0] == 'v') return version;
    if (version[0] == 'V') return "v" + version.substr(1);
    if (version[0] >= '0' && version[0] <= '9') return "v" + version;
    return version;
}

} // namespace format
