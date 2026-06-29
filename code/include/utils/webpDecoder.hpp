/**
 * webpDecoder - WebP 图片解码工具
 * 将 WebP 二进制数据解码为 RGBA 像素数据
 * 纯 header，inline，无框架依赖
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <webp/decode.h>

namespace webpDecoder {

struct WebpImage {
    std::vector<uint8_t> pixels;
    int width  = 0;
    int height = 0;
};

/**
 * @brief 将 WebP 二进制数据解码为 RGBA 像素
 * @param data WebP 图片的原始字节指针
 * @param size 数据长度（字节）
 * @return WebpImage 包含像素数据和尺寸，失败时 width 为 0
 */
inline WebpImage decode(const uint8_t* data, size_t size) {
    int w, h;
    uint8_t* rgba = WebPDecodeRGBA(data, size, &w, &h);
    if (!rgba) return {};
    WebpImage img{std::vector<uint8_t>(rgba, rgba + w * h * 4), w, h};
    WebPFree(rgba);
    return img;
}

} // namespace webpDecoder
