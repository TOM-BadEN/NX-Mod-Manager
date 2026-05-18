/**
 * QrCode - 二维码图片组件实现
 *
 * 参考 CircleButton 设计，使用 inflateFromXMLString 内联布局。
 * 构造时创建居中 brls::Image，通过 text 属性触发二维码生成。
 */

#include "view/qrCode.hpp"
#include "utils/qrcode.hpp"

const std::string qrCodeXML = R"xml(
    <brls:Box
        width="auto"
        height="auto"
        justifyContent="center"
        alignItems="center">

        <brls:Image
            id="qrCode/image"
            width="100%"
            height="100%"/>

    </brls:Box>
)xml";

QrCode::QrCode()
{
    this->inflateFromXMLString(qrCodeXML);
    m_image = (brls::Image*)getView("qrCode/image");
    m_image->setFreeTexture(false);

    registerStringXMLAttribute("text", [this](const std::string& value) {
        setText(value);
    });
}

QrCode::~QrCode()
{
    if (m_texId > 0) nvgDeleteImage(brls::Application::getNVGContext(), m_texId);
}

void QrCode::setText(const std::string& text)
{
    if (m_texId > 0) {
        nvgDeleteImage(brls::Application::getNVGContext(), m_texId);
        m_texId = 0;
    }

    auto qr = qrcode::generate(text);
    auto* vg = brls::Application::getNVGContext();
    m_texId = nvgCreateImageRGBA(vg, qr.size, qr.size, 0, qr.pixels.data());
    if (m_texId > 0) m_image->innerSetImage(m_texId);
}

brls::View* QrCode::create()
{
    return new QrCode();
}
