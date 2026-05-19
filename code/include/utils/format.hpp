/**
 * format - 通用格式化工具函数
 * 纯 header，inline，无业务逻辑
 */

#pragma once

#include <string>
#include <cstdio>
#include <cstdint>
#include <ctime>
#include <borealis/core/i18n.hpp>

namespace format {

    /**
     * @brief appId → 16位大写十六进制字符串
     * @param appId 游戏 appId
     */
    inline std::string appIdHex(uint64_t appId) {
        char buf[17];
        std::snprintf(buf, sizeof(buf), "%016lX", appId);
        return std::string(buf);
    }

    /**
     * @brief TID 十六进制字符串 → uint64_t
     * @param hex 16位十六进制字符串
     */
    inline uint64_t appIdFromHex(const std::string& hex) {
        char* end = nullptr;
        uint64_t val = std::strtoull(hex.c_str(), &end, 16);
        if (end == hex.c_str() || *end != '\0' || val == 0) return 0;
        return val;
    }

    /**
     * @brief 校验 appId 是否为合法的游戏 TID
     * @param appId 待校验的 appId
     */
    inline bool appIdIsValid(uint64_t appId) {
        return ((appId >> 48) == 0x0100) && ((appId & 0x1FFF) == 0);
    }

    /**
     * @brief 字节数 → 可读体积字符串（如 "12.5 MB"）
     * @param bytes 字节数
     */
    inline std::string fileSize(int64_t bytes) {
        char buf[32];
        if (bytes < 1024) std::snprintf(buf, sizeof(buf), "%lld B", static_cast<long long>(bytes));
        else if (bytes < 1024 * 1024) std::snprintf(buf, sizeof(buf), "%.1f KB", bytes / 1024.0);
        else if (bytes < 1024LL * 1024 * 1024) std::snprintf(buf, sizeof(buf), "%.1f MB", bytes / (1024.0 * 1024));
        else std::snprintf(buf, sizeof(buf), "%.1f GB", bytes / (1024.0 * 1024 * 1024));
        return std::string(buf);
    }

    /**
     * @brief 毫秒 → 可读时间字符串
     * @param ms 毫秒数
     */
    inline std::string elapsed(int64_t ms) {
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

    /**
     * @brief libnx Result → 十六进制字符串
     * @param result libnx Result 错误码
     */
    inline std::string resultHex(uint32_t result) {
        char buf[11];
        std::snprintf(buf, sizeof(buf), "0x%08X", result);
        return std::string(buf);
    }

    /**
     * @brief 字节/秒 → 可读速度字符串
     * @param bytesPerSecond 每秒字节数
     */
    inline std::string transferSpeed(double bytesPerSecond) {
        char buf[32];
        if (bytesPerSecond < 1024) std::snprintf(buf, sizeof(buf), "%.0f B/s", bytesPerSecond);
        else if (bytesPerSecond < 1024 * 1024) std::snprintf(buf, sizeof(buf), "%.1f KB/s", bytesPerSecond / 1024.0);
        else if (bytesPerSecond < 1024.0 * 1024 * 1024) std::snprintf(buf, sizeof(buf), "%.1f MB/s", bytesPerSecond / (1024.0 * 1024));
        else std::snprintf(buf, sizeof(buf), "%.1f GB/s", bytesPerSecond / (1024.0 * 1024 * 1024));
        return std::string(buf);
    }

    /**
     * @brief 秒数 → 按格式模板输出时间字符串
     * @param seconds 秒数
     * @param format 格式模板（"HH:MM:SS"、"MM:SS"、"SS"）
     */
    inline std::string duration(int seconds, const std::string& format) {
        char buf[16];
        int h = seconds / 3600;
        int m = (seconds % 3600) / 60;
        int s = seconds % 60;
        if (format == "HH:MM:SS") std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", h, m, s);
        else if (format == "MM:SS") std::snprintf(buf, sizeof(buf), "%02d:%02d", m, s);
        else std::snprintf(buf, sizeof(buf), "%02d", s);
        return std::string(buf);
    }

    /**
     * @brief 日期字符串 → 相对时间（如 "刚刚"、"5分钟前"、"3天前"）
     * @param dateString 日期字符串（"YYYY-MM-DD HH:MM:SS" 或 "YYYY-MM-DDTHH:MM:SS"）
     */
    inline std::string timeAgo(const std::string& dateString) {
        if (dateString.empty()) return "";

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

        std::time_t target = std::mktime(&tm);
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

    /**
     * @brief 从 dirPath 提取游戏目录名
     * @param dirPath 格式：/mods2/{gameDirName}/{appIdHex}
     */
    inline std::string gameDirName(const std::string& dirPath) {
        auto pos2 = dirPath.rfind('/');
        if (pos2 != std::string::npos && pos2 > 0) {
            auto pos1 = dirPath.rfind('/', pos2 - 1);
            return dirPath.substr(pos1 + 1, pos2 - pos1 - 1);
        }
        return {};
    }

    /**
     * @brief 版本号字符串 → 整数（如 "v3.2.1" → 3002001，每段占三位）
     * @param version 版本号字符串（支持 "v" 前缀）
     */
    inline long versionCode(const std::string& version) {
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

    /**
     * @brief 清洗版本号（确保小写 v 前缀）
     * @param version 原始版本号字符串
     */
    inline std::string cleanVersion(const std::string& version) {
        if (version.empty()) return version;
        if (version[0] == 'v') return version;
        if (version[0] == 'V') return "v" + version.substr(1);
        if (version[0] >= '0' && version[0] <= '9') return "v" + version;
        return version;
    }

} // namespace format
