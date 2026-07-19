/**
 * qrcode - 二维码生成工具
 */

#include "utils/qrcode.hpp"
#include <qrcodegen.hpp>

namespace qrcode {

QrImage generate(const std::string& text, int scale, int border) {
    using qrcodegen::QrCode;

    QrCode qr = QrCode::encodeText(text.c_str(), QrCode::Ecc::MEDIUM);

    int qrSize = qr.getSize();
    int imgSize = (qrSize + border * 2) * scale;

    QrImage result;
    result.size = imgSize;
    result.pixels.assign(imgSize * imgSize * 4, 255);

    for (int y = 0; y < qrSize; y++) {
        for (int x = 0; x < qrSize; x++) {
            if (!qr.getModule(x, y)) continue;
            int ox = (x + border) * scale;
            int oy = (y + border) * scale;
            for (int dy = 0; dy < scale; dy++) {
                for (int dx = 0; dx < scale; dx++) {
                    int idx = ((oy + dy) * imgSize + (ox + dx)) * 4;
                    result.pixels[idx] = 0;
                    result.pixels[idx + 1] = 0;
                    result.pixels[idx + 2] = 0;
                    result.pixels[idx + 3] = 255;
                }
            }
        }
    }

    return result;
}

}
