/**
 * SVGImage - SVG 图片组件
 * 基于 lunasvg 实现矢量图标渲染，继承自 brls::Image
 *
 * 原作者: fang (wiliwili 项目)
 * 来源: https://github.com/xfangfang/wiliwili
 */

#pragma once

#include <borealis.hpp>
#include <lunasvg.h>

class SVGImage : public brls::Image {
public:
    SVGImage();
    ~SVGImage() override;

    /**
     * @brief 从资源加载 SVG
     * @param value 资源路径
     */
    void setImageFromSVGRes(const std::string& value);

    /**
     * @brief 从文件加载 SVG
     * @param value 文件路径
     */
    void setImageFromSVGFile(const std::string& value);

    /**
     * @brief 从字符串加载 SVG
     * @param value SVG 字符串
     */
    void setImageFromSVGString(const std::string& value);

    void onLayout() override;

    static brls::View* create();

private:
    /** @brief 更新位图 */
    void updateBitmap();

    std::unique_ptr<lunasvg::Document> document;
    float lastWidth = 0;
    float lastHeight = 0;
};
