/**
 * fsHelper - 文件系统操作封装
 * 基于 libnx 原生 API，纯工具函数，不含业务逻辑
 */

#pragma once

#include <string>
#include <vector>
#include <cstring>

// 路径包装：自动拷贝到栈上，避免堆指针传给 libnx IPC
struct FsPath {
    static constexpr int MAX_LEN = 0x301;  // 与 libnx FS_MAX_PATH 一致
    char s[MAX_LEN] = {};

    FsPath() = default;
    FsPath(const char* str) { std::strncpy(s, str, MAX_LEN - 1); }
    FsPath(const std::string& str) { std::strncpy(s, str.c_str(), MAX_LEN - 1); }

    operator const char*() const { return s; }
};

namespace fs {

    // 目录是否存在
    bool dirExists(const FsPath& path);

    // 确保目录存在，不存在则创建
    bool ensureDir(const FsPath& path);
              
    // 列出指定目录下的所有子目录名（不递归）
    std::vector<std::string> listSubDirs(const FsPath& path);

    // 只数子目录数量
    int countDirs(const FsPath& path);

    // 数指定目录下的条目数（子目录 + 文件）
    // exts 为空：数所有条目（快速路径）
    // exts 非空：数所有子目录 + 匹配扩展名的文件（exts 需小写，如 {".zip"}）
    int countItems(const FsPath& path, const std::vector<std::string>& exts = {});

} // namespace fs
