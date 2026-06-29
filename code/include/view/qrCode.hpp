/**
 * QrCode - 二维码图片组件
 *
 * 将文本编码为二维码并显示，继承自 brls::Box。
 * 内部包含一个 brls::Image，纹理自动管理。
 *
 * XML 用法：
 *   <QrCode text="https://example.com" width="300" height="300"/>
 *
 * 也可在代码中动态设置：
 *   qrCode->setText("https://example.com");
 */

#pragma once

#include <borealis.hpp>
#include <string>

class QrCode : public brls::Box
{
  public:
    QrCode();
    ~QrCode();

    /**
     * @brief 设置二维码文本
     * @param text 文本内容
     */
    void setText(const std::string& text);

    static brls::View* create();

  private:
    brls::Image* m_image;
    int m_texId = 0;
};
