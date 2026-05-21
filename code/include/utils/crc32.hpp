/**
 * crc32 - 文件 CRC32 计算
 * 基于 libnx 硬件加速 CRC32 + 原生 fs API
 */

#pragma once

#include <cstdint>
#include <cstring>
#include <switch.h>

namespace crc {

/**
 * @brief 计算文件的 CRC32，文件不存在返回 -1，存在返回 0~0xFFFFFFFF
 * @param path 文件路径
 * @param buf 读取缓冲区（调用方提供）
 * @param bufSize 缓冲区大小
 */
inline int64_t fromFile(const char* path, void* buf, size_t bufSize) {
    FsFileSystem* fs = fsdevGetDeviceFileSystem("sdmc:");
    if (!fs) return -1;

    char pathBuf[0x301] = {};
    std::strncpy(pathBuf, path, 0x300);

    FsFile file;
    if (R_FAILED(fsFsOpenFile(fs, pathBuf, FsOpenMode_Read, &file))) return -1;

    uint32_t crc = 0;
    int64_t offset = 0;
    u64 bytesRead = 0;

    while (true) {
        Result rc = fsFileRead(&file, offset, buf, bufSize, FsReadOption_None, &bytesRead);
        if (R_FAILED(rc) || bytesRead == 0) break;
        crc = crc32CalculateWithSeed(crc, buf, bytesRead);
        offset += static_cast<int64_t>(bytesRead);
    }

    fsFileClose(&file);
    return static_cast<int64_t>(crc);
}

} // namespace crc
