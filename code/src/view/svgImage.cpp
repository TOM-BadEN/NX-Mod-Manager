/**
 * SVGImage - SVG 图片组件
 * 基于 lunasvg 实现矢量图标渲染，继承自 brls::Image
 *
 * 原作者: fang (wiliwili 项目)
 * 来源: https://github.com/xfangfang/wiliwili
 */

#include "view/svgImage.hpp"

#ifdef USE_LIBROMFS
#include <romfs/romfs.hpp>
#endif

SVGImage::SVGImage() {
    this->registerStringXMLAttribute("SVG", [this](const std::string& value) {
        this->setImageFromSVGRes(value);
    });
}

SVGImage::~SVGImage() = default;

void SVGImage::setImageFromSVGRes(const std::string& value) {
#ifdef USE_LIBROMFS
    auto& data = romfs::get(value);
    this->document = lunasvg::Document::loadFromData((const char*)data.data(), data.size());
#else
    this->setImageFromSVGFile(std::string(BRLS_RESOURCES) + value);
    return;
#endif
    this->updateBitmap();
}

void SVGImage::setImageFromSVGFile(const std::string& value) {
    this->document = lunasvg::Document::loadFromFile(value);
    this->updateBitmap();
}

void SVGImage::setImageFromSVGString(const std::string& value) {
    this->document = lunasvg::Document::loadFromData(value);
    this->updateBitmap();
}

void SVGImage::onLayout() {
    brls::Image::onLayout();
    float width = this->getWidth();
    float height = this->getHeight();
    if (width != this->lastWidth || height != this->lastHeight) {
        this->lastWidth = width;
        this->lastHeight = height;
        this->updateBitmap();
    }
}

void SVGImage::updateBitmap() {
    if (!this->document) return;

    float width = this->getWidth();
    float height = this->getHeight();
    if (width <= 0 || height <= 0) return;

    float scale = brls::Application::windowScale;
    int renderWidth = (int)(width * scale);
    int renderHeight = (int)(height * scale);
    if (renderWidth <= 0 || renderHeight <= 0) return;

    auto bitmap = this->document->renderToBitmap(renderWidth, renderHeight);
    if (!bitmap.valid()) return;

    // lunasvg 输出 BGRA，NanoVG 期望 RGBA，交换 R 和 B 通道
    uint8_t* pixels = bitmap.data();
    int pixelCount = renderWidth * renderHeight;
    for (int i = 0; i < pixelCount; i++) {
        std::swap(pixels[i * 4], pixels[i * 4 + 2]);
    }

    NVGcontext* vg = brls::Application::getNVGContext();
    int tex = nvgCreateImageRGBA(vg, renderWidth, renderHeight, 0, pixels);
    if (tex > 0) {
        this->innerSetImage(tex);
    }
}

brls::View* SVGImage::create() {
    return new SVGImage();
}
