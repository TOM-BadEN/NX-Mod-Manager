/**
 * fsHelper - 文件系统操作封装
 * 基于 libnx 原生 API，纯工具函数，不含业务逻辑
 */

#pragma once

#include <string>
#include <vector>

namespace fs {

    // 目录是否存在
    bool dirExists(const std::string& path);

    // 列出指定目录下的所有子目录名（不递归）
    std::vector<std::string> listSubDirs(const std::string& path);

    // 数指定目录下的条目数（子目录 + 文件）
    // exts 为空：数所有条目（快速路径）
    // exts 非空：数所有子目录 + 匹配扩展名的文件（exts 需小写，如 {".zip"}）
    int countItems(const std::string& path, const std::vector<std::string>& exts = {});

} // namespace fs
