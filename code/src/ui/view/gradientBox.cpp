/**
 * GradientBox - 带有序抖动渐变背景的通用容器实现
 */

#include "ui/view/gradientBox.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace {

constexpr int DITHER_SIZE = 8;

// 8×8 Bayer 矩阵。在写入纹理前分散颜色量化误差，避免形成色带。
constexpr std::array<unsigned char, DITHER_SIZE * DITHER_SIZE> DITHER_PATTERN = {
    0, 48, 12, 60, 3, 51, 15, 63,
    32, 16, 44, 28, 35, 19, 47, 31,
    8, 56, 4, 52, 11, 59, 7, 55,
    40, 24, 36, 20, 43, 27, 39, 23,
    2, 50, 14, 62, 1, 49, 13, 61,
    34, 18, 46, 30, 33, 17, 45, 29,
    10, 58, 6, 54, 9, 57, 5, 53,
    42, 26, 38, 22, 41, 25, 37, 21,
};

unsigned char quantizeWithDither(float value, float threshold) {
    float scaled = std::clamp(value, 0.0f, 1.0f) * 255.0f;
    int quantized = static_cast<int>(scaled);
    if (quantized < 255 && scaled - static_cast<float>(quantized) > threshold) quantized++;
    return static_cast<unsigned char>(quantized);
}

} // namespace

GradientBox::GradientBox() {
    registerColorXMLAttribute("gradientStartColor", [this](NVGcolor color) { setGradientStartColor(color); });
    registerColorXMLAttribute("gradientEndColor", [this](NVGcolor color) { setGradientEndColor(color); });
    BRLS_REGISTER_ENUM_XML_ATTRIBUTE("gradientDirection", GradientDirection, this->setGradientDirection, {{ "vertical", GradientDirection::Vertical }, { "horizontal", GradientDirection::Horizontal }});
}

GradientBox::~GradientBox() {
    releaseGradientTexture(brls::Application::getNVGContext());
}

void GradientBox::setGradientStartColor(NVGcolor color) {
    m_gradientStartColor = color;
    m_hasGradientStartColor = true;
    m_textureDirty = true;
}

void GradientBox::setGradientEndColor(NVGcolor color) {
    m_gradientEndColor = color;
    m_hasGradientEndColor = true;
    m_textureDirty = true;
}

void GradientBox::setGradientDirection(GradientDirection direction) {
    if (m_gradientDirection == direction) return;
    m_gradientDirection = direction;
    m_textureDirty = true;
}

void GradientBox::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) {
    if (m_hasGradientStartColor && m_hasGradientEndColor) {
        ensureGradientTexture(vg, width, height);

        if (m_texture > 0) {
            NVGpaint gradient;
            if (m_gradientDirection == GradientDirection::Vertical) gradient = nvgImagePattern(vg, x, y, DITHER_SIZE, height, 0.0f, m_texture, 1.0f);
            else gradient = nvgImagePattern(vg, x, y, width, DITHER_SIZE, 0.0f, m_texture, 1.0f);

            float borderInset = getBorderThickness() * 0.5f;
            float fillWidth = width - borderInset * 2.0f;
            float fillHeight = height - borderInset * 2.0f;
            if (fillWidth > 0.0f && fillHeight > 0.0f) {
                nvgSave(vg);
                nvgBeginPath(vg);
                float radius = std::max(0.0f, getCornerRadius() - borderInset);
                if (radius > 0.0f) nvgRoundedRect(vg, x + borderInset, y + borderInset, fillWidth, fillHeight, radius);
                else nvgRect(vg, x + borderInset, y + borderInset, fillWidth, fillHeight);
                nvgFillPaint(vg, a(gradient));
                nvgFill(vg);
                nvgRestore(vg);
            }
        }
    }

    brls::Box::draw(vg, x, y, width, height, style, ctx);
}

brls::View* GradientBox::create() {
    return new GradientBox();
}

void GradientBox::ensureGradientTexture(NVGcontext* vg, float width, float height) {
    int gradientLength = static_cast<int>(std::ceil(m_gradientDirection == GradientDirection::Vertical ? height : width));
    if (gradientLength <= 0) return;

    int expectedWidth = m_gradientDirection == GradientDirection::Vertical ? DITHER_SIZE : gradientLength;
    int expectedHeight = m_gradientDirection == GradientDirection::Vertical ? gradientLength : DITHER_SIZE;
    if (!m_textureDirty && m_texture > 0 && m_textureWidth == expectedWidth && m_textureHeight == expectedHeight) return;

    std::vector<unsigned char> pixels(static_cast<std::size_t>(expectedWidth) * static_cast<std::size_t>(expectedHeight) * 4);

    for (int pixelY = 0; pixelY < expectedHeight; pixelY++) {
        for (int pixelX = 0; pixelX < expectedWidth; pixelX++) {
            int gradientPosition = m_gradientDirection == GradientDirection::Vertical ? pixelY : pixelX;
            float progress = gradientLength > 1 ? static_cast<float>(gradientPosition) / static_cast<float>(gradientLength - 1) : 0.0f;
            float threshold = (static_cast<float>(DITHER_PATTERN[static_cast<std::size_t>(pixelY % DITHER_SIZE) * DITHER_SIZE + static_cast<std::size_t>(pixelX % DITHER_SIZE)]) + 0.5f) / static_cast<float>(DITHER_SIZE * DITHER_SIZE);
            std::size_t pixelIndex = (static_cast<std::size_t>(pixelY) * static_cast<std::size_t>(expectedWidth) + static_cast<std::size_t>(pixelX)) * 4;

            pixels[pixelIndex] = quantizeWithDither(m_gradientStartColor.r + (m_gradientEndColor.r - m_gradientStartColor.r) * progress, threshold);
            pixels[pixelIndex + 1] = quantizeWithDither(m_gradientStartColor.g + (m_gradientEndColor.g - m_gradientStartColor.g) * progress, threshold);
            pixels[pixelIndex + 2] = quantizeWithDither(m_gradientStartColor.b + (m_gradientEndColor.b - m_gradientStartColor.b) * progress, threshold);
            pixels[pixelIndex + 3] = quantizeWithDither(m_gradientStartColor.a + (m_gradientEndColor.a - m_gradientStartColor.a) * progress, threshold);
        }
    }

    int imageFlags = NVG_IMAGE_NEAREST;
    if (m_gradientDirection == GradientDirection::Vertical) imageFlags |= NVG_IMAGE_REPEATX;
    else imageFlags |= NVG_IMAGE_REPEATY;

    int newTexture = nvgCreateImageRGBA(vg, expectedWidth, expectedHeight, imageFlags, pixels.data());
    if (newTexture <= 0) return;

    releaseGradientTexture(vg);
    m_texture = newTexture;
    m_textureWidth = expectedWidth;
    m_textureHeight = expectedHeight;
    m_textureDirty = false;
}

void GradientBox::releaseGradientTexture(NVGcontext* vg) {
    if (m_texture <= 0) return;
    nvgDeleteImage(vg, m_texture);
    m_texture = 0;
    m_textureWidth = 0;
    m_textureHeight = 0;
}
