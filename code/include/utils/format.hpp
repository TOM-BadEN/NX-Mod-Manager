/**
 * format - 通用格式化工具函数
 * 纯 header，inline，无业务逻辑
 */

#pragma once

#include <string>
#include <cstdio>
#include <cstdint>

namespace format {

    // appId → 16位大写十六进制字符串（如 "0100C510166F0000"）
    inline std::string appIdHex(uint64_t appId) {
        char buf[17];
        std::snprintf(buf, sizeof(buf), "%016lX", appId);
        return std::string(buf);
    }

    // 字节数 → 可读体积字符串（如 "12.5 MB"）
    inline std::string fileSize(int64_t bytes) {
        char buf[32];
        if (bytes < 1024) std::snprintf(buf, sizeof(buf), "%lld B", static_cast<long long>(bytes));
        else if (bytes < 1024 * 1024) std::snprintf(buf, sizeof(buf), "%.1f KB", bytes / 1024.0);
        else if (bytes < 1024LL * 1024 * 1024) std::snprintf(buf, sizeof(buf), "%.1f MB", bytes / (1024.0 * 1024));
        else std::snprintf(buf, sizeof(buf), "%.1f GB", bytes / (1024.0 * 1024 * 1024));
        return std::string(buf);
    }

} // namespace format
