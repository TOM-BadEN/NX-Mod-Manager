/**
 * format - 通用格式化工具函数
 * 声明格式化、时间、版本号等无状态工具函数
 */

#pragma once

#include <cstdint>
#include <string>

namespace format {

    /**
     * @brief appId → 16位大写十六进制字符串
     * @param appId 游戏 appId
     */
    std::string appIdHex(uint64_t appId);

    /**
     * @brief TID 十六进制字符串 → uint64_t
     * @param hex 16位十六进制字符串
     */
    uint64_t appIdFromHex(const std::string& hex);

    /**
     * @brief 校验 appId 是否为合法的游戏 TID
     * @param appId 待校验的 appId
     */
    bool appIdIsValid(uint64_t appId);

    /**
     * @brief 字节数 → 可读体积字符串（如 "12.5 MB"）
     * @param bytes 字节数
     */
    std::string fileSize(int64_t bytes);

    /**
     * @brief 毫秒 → 可读时间字符串
     * @param ms 毫秒数
     */
    std::string elapsed(int64_t ms);

    /**
     * @brief libnx Result → 十六进制字符串
     * @param result libnx Result 错误码
     */
    std::string resultHex(uint32_t result);

    /**
     * @brief 字节/秒 → 可读速度字符串
     * @param bytesPerSecond 每秒字节数
     */
    std::string transferSpeed(double bytesPerSecond);

    /**
     * @brief 秒数 → 按格式模板输出时间字符串
     * @param seconds 秒数
     * @param format 格式模板（"HH:MM:SS"、"MM:SS"、"SS"）
     */
    std::string duration(int seconds, const std::string& format);

    /**
     * @brief 日期字符串 → 相对时间（如 "刚刚"、"5分钟前"、"3天前"）
     * @param dateString 日期字符串（无时区按本地时间，带 Z / +/-HH:MM 按实际时区）
     */
    std::string timeAgo(const std::string& dateString);

    /**
     * @brief 从 dirPath 提取游戏目录名
     * @param dirPath 格式：/mods2/{gameDirName}/{appIdHex}
     */
    std::string gameDirName(const std::string& dirPath);

    /**
     * @brief 版本号字符串 → 整数（如 "v3.2.1" → 3002001，每段占三位）
     * @param version 版本号字符串（支持 "v" 前缀）
     */
    long versionCode(const std::string& version);

    /**
     * @brief 单换行扩展为空行，已有连续换行保持不变
     * @param text 原始文本
     */
    std::string addBlankLineAfterLineBreaks(const std::string& text);

    /**
     * @brief 清洗版本号（确保小写 v 前缀）
     * @param version 原始版本号字符串
     */
    std::string cleanVersion(const std::string& version);

} // namespace format
