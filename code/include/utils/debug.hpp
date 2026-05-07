/**
 * debug - 调试日志工具
 * 通过 NXLINK 宏控制日志输出，关闭时零开销
 * 输出格式与 brls::Logger 完全一致
 * 
 * 用法：
 *   LOG("普通信息 %d", val);
 *   LOG_W("警告 %s", msg);
 *   LOG_E("错误 code=%d", err);
 *   LOG_D("调试数据 x=%f", x);
 * 
 * 编译：
 *   make        → 发布版（所有日志不执行）
 *   make debug  → 调试版（日志输出到 nxlink）
 */

#pragma once

#ifdef NXLINK
#include <cstdio>
#include <chrono>
#include <ctime>

#define _LOG_FILE (__builtin_strrchr(__FILE__, '/') ? __builtin_strrchr(__FILE__, '/') + 1 : __FILE__)

#define _LOG_IMPL(color, tag, fmt, ...) do { \
    auto _now = std::chrono::system_clock::now(); \
    auto _ms = std::chrono::duration_cast<std::chrono::milliseconds>( \
        _now.time_since_epoch()).count() % 1000; \
    std::time_t _tt = std::chrono::system_clock::to_time_t(_now); \
    std::tm _tm = *std::localtime(&_tt); \
    printf("%02d:%02d:%02d.%03d\033" color "[" tag "]\033[0m %s:%d " fmt "\n", \
        _tm.tm_hour, _tm.tm_min, _tm.tm_sec, (int)_ms, _LOG_FILE, __LINE__, ##__VA_ARGS__); \
} while(0)

#define LOG(fmt, ...)    _LOG_IMPL("[0;34m", "INFO",    fmt, ##__VA_ARGS__)
#define LOG_W(fmt, ...)  _LOG_IMPL("[0;33m", "WARNING", fmt, ##__VA_ARGS__)
#define LOG_E(fmt, ...)  _LOG_IMPL("[0;31m", "ERROR",   fmt, ##__VA_ARGS__)
#define LOG_D(fmt, ...)  _LOG_IMPL("[0;32m", "DEBUG",   fmt, ##__VA_ARGS__)

#else
#define LOG(fmt, ...)    ((void)0)
#define LOG_W(fmt, ...)  ((void)0)
#define LOG_E(fmt, ...)  ((void)0)
#define LOG_D(fmt, ...)  ((void)0)
#endif
