/**
 * imageDecoder - 通用图片解码工具
 *
 * 将 JPEG、PNG 和 WebP 二进制数据解码为 RGBA 像素数据。
 * 只负责 CPU 解码，不创建 NanoVG 纹理，也不操作纹理缓存。
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace imageDecoder {

struct DecodedImage {
    std::vector<uint8_t> pixels;
    int width = 0;
    int height = 0;
};

/** @brief 将 JPEG 二进制数据解码为 RGBA 像素 */
DecodedImage decodeJpeg(const uint8_t* data, size_t size);

/** @brief 将 PNG 二进制数据解码为 RGBA 像素 */
DecodedImage decodePng(const uint8_t* data, size_t size);

/** @brief 将 WebP 二进制数据解码为 RGBA 像素 */
DecodedImage decodeWebp(const uint8_t* data, size_t size);

} // namespace imageDecoder
