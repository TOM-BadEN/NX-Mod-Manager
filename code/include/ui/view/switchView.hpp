/**
 * SwitchView - 开关控件（轨道 + 圆形滑块）
 *
 * 纯 NanoVG 自绘，不持有任何纹理；状态切换时滑块沿轨道平移，
 * 轨道颜色随状态在关闭灰与主题强调色之间渐变。
 */

#pragma once

#include <borealis.hpp>

class SwitchView : public brls::View {
public:
    SwitchView();

    /**
     * @brief 设置开关状态
     * @param on 是否开启
     * @param animated 是否播放滑块动画
     */
    void setOn(bool on, bool animated = true);

    /**
     * @brief 查询开关是否正在播放切换动画
     * @return 动画播放中返回 true
     */
    bool isAnimating();

    /**
     * @brief 设置是否可用（禁用时整体变暗）
     * @param enabled 是否可用
     */
    void setEnabled(bool enabled);

    /**
     * @brief 绘制开关
     * @param vg NanoVG 上下文
     * @param x 横坐标
     * @param y 纵坐标
     * @param width 宽度
     * @param height 高度
     * @param style 当前样式
     * @param ctx 帧上下文
     */
    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) override;

private:
    static constexpr float TRACK_WIDTH = 64.0f;   // 轨道宽度
    static constexpr float TRACK_HEIGHT = 32.0f;  // 轨道高度
    static constexpr float KNOB_SIZE = 26.0f;     // 滑块直径
    static constexpr float KNOB_PADDING = 3.0f;   // 滑块距轨道边缘间距

    bool m_on = false;                     // 当前状态
    bool m_enabled = true;                 // 是否可用
    brls::Animatable m_knobProgress{0.0f}; // 滑块位置进度（0=左，1=右）
};
