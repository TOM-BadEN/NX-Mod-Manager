/**
 * ImageViewer - 图片浏览组件
 *
 * 全屏暗色遮罩 + 居中大图，用于浏览截图等图片。
 * 切换图片时带平移滑动动画。
 * 关闭方式：B 键 / A 键 / 触摸点击
 */

#pragma once

#include <borealis.hpp>
#include <vector>

class ImageViewer : public brls::Box {
public:
    /**
     * @brief 构造图片浏览器
     * @param textureIds 纹理 ID 列表
     * @param sourceImages 与纹理一一对应的来源图片控件，不持有所有权
     * @param startIndex 初始显示索引
     */
    ImageViewer(const std::vector<int>& textureIds, const std::vector<brls::Image*>& sourceImages, int startIndex);

    /** @brief 获取图片浏览器的默认焦点 */
    brls::View* getDefaultFocus() override;

    /** @brief 图片浏览器不使用 AppletFrame */
    brls::AppletFrame* getAppletFrame() override;

    /** @brief 图片浏览器使用透明 Activity 显示下层页面 */
    bool isTranslucent() override { return true; }

    /**
     * @brief 绘制图片浏览器和缩放过渡动画
     * @param vg NanoVG 上下文
     * @param x 组件横坐标
     * @param y 组件纵坐标
     * @param width 组件宽度
     * @param height 组件高度
     * @param style 当前界面样式
     * @param ctx 当前帧上下文
     */
    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) override;

    /**
     * @brief 打开图片浏览器
     * @param textureIds 纹理 ID 列表
     * @param sourceImages 与纹理一一对应的来源图片控件，不持有所有权
     * @param startIndex 初始显示索引
     */
    static void open(const std::vector<int>& textureIds, const std::vector<brls::Image*>& sourceImages, int startIndex = 0);

    /** @brief 关闭图片浏览器 */
    static void close();

private:
    /** @brief 图片浏览器当前交互状态 */
    enum class ViewerState {
        Opening,    // 正在从来源图片展开
        Idle,       // 动画结束，允许接收操作
        Navigating, // 正在切换图片或播放边界抖动
        Closing,    // 正在向来源图片缩回
    };

    static constexpr int OPEN_DURATION = 300;          // 展开动画持续时间（毫秒）
    static constexpr int CLOSE_DURATION = 250;         // 缩回动画持续时间（毫秒）
    static constexpr int SLIDE_DURATION = 400;         // 图片切换动画持续时间（毫秒）
    static constexpr float IMAGE_MAX_RATIO = 0.75f;    // 大图占屏幕宽高的最大比例

    std::vector<brls::Label*> m_dots;                   // 底部图片位置指示器
    std::vector<int> m_textureIds;                      // 可浏览的纹理 ID 列表
    std::vector<brls::Image*> m_sourceImages;           // 与纹理对应的来源图片控件，不持有所有权
    int m_currentIndex = 0;                             // 当前显示的图片索引
    ViewerState m_state = ViewerState::Opening;         // 当前交互状态
    brls::Rect m_sourceFrame;                           // 当前来源图片在屏幕中的区域
    brls::Rect m_targetFrame;                           // 浏览器大图在屏幕中的区域
    NVGcolor m_backdropColor;                           // 完整显示时的背景遮罩颜色
    brls::Animatable m_transitionProgress{0.0f};        // 展开或缩回动画进度
    brls::Animatable m_offsetCur{0.0f};                 // 当前图片切换时的横向偏移
    brls::Animatable m_offsetNext{0.0f};                // 下一张图片切换时的横向偏移

    static ImageViewer* s_current;                      // 当前打开的图片浏览器，不持有所有权

    /** @brief 初始化指示器 */
    void setupIndicator();

    /** @brief 初始化按键操作 */
    void setupActions();

    /**
     * @brief 导航切换
     * @param right 是否向右
     */
    void navigate(bool right);

    /**
     * @brief 切换图片（带动画）
     * @param index 目标索引
     * @param slideLeft 是否左滑
     */
    void switchImage(int index, bool slideLeft);

    /**
     * @brief 设置图片纹理，并把控件尺寸调整到等比适配后的真实显示尺寸
     * @param image 目标图片控件
     * @param textureId 纹理 ID
     */
    void setImageTextureAndFitSize(brls::Image* image, int textureId);

    /** @brief 开始从缩略图展开 */
    void startOpenTransition();

    /** @brief 开始向当前缩略图缩回 */
    void startCloseTransition();

    /** @brief 根据动画进度更新背景透明度 */
    void updateTransition();

    /**
     * @brief 缩放过渡结束回调
     * @param finished 动画是否正常完成
     */
    void finishTransition(bool finished);

    /**
     * @brief 边界抖动动画
     * @param right 是否向右
     */
    void shakeImage(bool right);

    /** @brief 更新指示器状态 */
    void updateIndicator();

    // XML 绑定的组件
    BRLS_BIND(brls::Image, m_image, "imageViewer/image");          // 当前显示的图片
    BRLS_BIND(brls::Image, m_imageNext, "imageViewer/imageNext"); // 图片切换时进入的下一张图片
    BRLS_BIND(brls::Box, m_indicator, "imageViewer/indicator");   // 底部图片位置指示器容器
};
