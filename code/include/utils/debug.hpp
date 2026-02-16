/**
 * debug - 调试日志工具
 * 通过 NXLINK 宏控制日志输出，关闭时零开销
 * 
 * 用法：
 *   LOG("hello %s, count=%d", name, n);
 * 
 * 编译：
 *   make        → 发布版（LOG 不执行）
 *   make debug  → 调试版（LOG 输出到 nxlink）
 */

#pragma once

#ifdef NXLINK
#include <cstdio>
#define LOG(fmt, ...)  printf(fmt "\n", ##__VA_ARGS__)
#else
#define LOG(fmt, ...)  ((void)0)
#endif
