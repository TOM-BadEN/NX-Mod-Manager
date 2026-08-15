/**
 * imageDecoder - 通用图片解码工具实现
 */

#include "utils/imageDecoder.hpp"
#include <borealis/extern/nanovg/stb_image.h>
#include <webp/decode.h>

namespace {

imageDecoder::DecodedImage decodeStb(const uint8_t* data, size_t size) {
    if (!data || size == 0) return {};

    int width = 0;
    int height = 0;
    int channels = 0;
    uint8_t* rgba = stbi_load_from_memory(data, static_cast<int>(size), &width, &height, &channels, 4);
    if (!rgba) return {};

    imageDecoder::DecodedImage image;
    image.pixels.assign(rgba, rgba + static_cast<size_t>(width) * height * 4);
    image.width = width;
    image.height = height;
    stbi_image_free(rgba);
    return image;
}

} // namespace

namespace imageDecoder {

DecodedImage decodeJpeg(const uint8_t* data, size_t size) {
    return decodeStb(data, size);
}

DecodedImage decodePng(const uint8_t* data, size_t size) {
    return decodeStb(data, size);
}

DecodedImage decodeWebp(const uint8_t* data, size_t size) {
    if (!data || size == 0) return {};

    int width = 0;
    int height = 0;
    uint8_t* rgba = WebPDecodeRGBA(data, size, &width, &height);
    if (!rgba) return {};

    DecodedImage image;
    image.pixels.assign(rgba, rgba + static_cast<size_t>(width) * height * 4);
    image.width = width;
    image.height = height;
    WebPFree(rgba);
    return image;
}

} // namespace imageDecoder
