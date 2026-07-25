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

class ImageViewer : public brls::Box
{
  public:
    /**
     * @brief 构造图片浏览器
     * @param textureIds 纹理 ID 列表
     * @param startIndex 初始显示索引
     */
    ImageViewer(const std::vector<int>& textureIds, int startIndex);

    brls::View* getDefaultFocus() override;
    brls::AppletFrame* getAppletFrame() override;
    bool isTranslucent() override { return true; }

    /**
     * @brief 打开图片浏览器
     * @param textureIds 纹理 ID 列表
     * @param startIndex 初始显示索引
     */
    static void open(const std::vector<int>& textureIds, int startIndex = 0);

    /** @brief 关闭图片浏览器 */
    static void close();

  private:
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

    /**
     * @brief 边界抱动动画
     * @param right 是否向右
     */
    void shakeImage(bool right);

    /** @brief 更新指示器状态 */
    void updateIndicator();

    BRLS_BIND(brls::Image, m_image, "imageViewer/image");
    BRLS_BIND(brls::Image, m_imageNext, "imageViewer/imageNext");
    BRLS_BIND(brls::Box, m_indicator, "imageViewer/indicator");

    std::vector<brls::Label*> m_dots;
    std::vector<int> m_textureIds;
    int m_currentIndex = 0;
    bool m_animating = false;

    brls::Animatable m_offsetCur{0.0f};
    brls::Animatable m_offsetNext{0.0f};

    static ImageViewer* s_current;
};
