/**
 * qrcode - 二维码生成工具
 * 将文本编码为二维码，返回 RGBA 像素数据
 */

#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace qrcode {

struct QrImage {
    std::vector<uint8_t> pixels;  // RGBA 像素数据
    int size = 0;                 // 图片宽高（正方形）
};

/**
 * 将文本生成二维码 RGBA 像素数据
 * @param text   要编码的文本（URL、字符串等）
 * @param scale  每个模块的像素大小（默认 8）
 * @param border 白色边框宽度，单位为模块数（默认 2）
 * @return QrImage 包含像素数据和尺寸，失败时 size 为 0
 */
QrImage generate(const std::string& text, int scale = 8, int border = 2);

}
