/**
 * GradientBox - 带有序抖动渐变背景的通用容器
 *
 * 继承 brls::Box 的全部布局能力，通过 XML 属性配置渐变颜色和方向。
 * 渐变纹理由组件按需创建、跨帧复用，并在尺寸或样式变化时自动更新。
 *
 * XML 用法：
 *   <GradientBox
 *       gradientStartColor="#181A1A"
 *       gradientEndColor="#FFFFFF"
 *       gradientDirection="vertical">
 *       <!-- 子元素 -->
 *   </GradientBox>
 */

#pragma once

#include <borealis.hpp>

enum class GradientDirection {
    Vertical,
    Horizontal,
};

class GradientBox : public brls::Box {
public:
    GradientBox();
    ~GradientBox() override;

    /** @brief 设置渐变起始颜色 */
    void setGradientStartColor(NVGcolor color);

    /** @brief 设置渐变结束颜色 */
    void setGradientEndColor(NVGcolor color);

    /** @brief 设置渐变方向 */
    void setGradientDirection(GradientDirection direction);

    /** @brief 绘制渐变背景和 Box 子元素 */
    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) override;

    /** @brief XML View 工厂函数 */
    static brls::View* create();

private:
    NVGcolor m_gradientStartColor = nvgRGBA(0, 0, 0, 0);                 // 渐变起始颜色
    NVGcolor m_gradientEndColor = nvgRGBA(0, 0, 0, 0);                   // 渐变结束颜色
    GradientDirection m_gradientDirection = GradientDirection::Vertical; // 当前渐变方向
    bool m_hasGradientStartColor = false;                                // 是否已经设置起始颜色
    bool m_hasGradientEndColor = false;                                  // 是否已经设置结束颜色
    bool m_textureDirty = true;                                          // 渐变纹理是否需要重新生成
    int m_texture = 0;                                                   // 当前渐变纹理 ID
    int m_textureWidth = 0;                                              // 当前渐变纹理宽度
    int m_textureHeight = 0;                                             // 当前渐变纹理高度

    /** @brief 创建或更新与当前颜色、方向和尺寸匹配的渐变纹理 */
    void ensureGradientTexture(NVGcontext* vg, float width, float height);

    /** @brief 释放当前渐变纹理 */
    void releaseGradientTexture(NVGcontext* vg);
};
