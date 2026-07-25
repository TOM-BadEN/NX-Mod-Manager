/**
 * QrCodeView - 二维码全屏展示组件
 *
 * 全屏半透明遮罩 + 白色圆角卡片 + 居中二维码。
 * A 键确认、B 键返回和触摸遮罩均关闭。
 *
 * 用法：
 *   QrCodeView::show("https://example.com");
 *   QrCodeView::show("https://example.com", "自定义提示");
 */

#pragma once

#include "ui/view/qrCode.hpp"
#include <borealis.hpp>
#include <string>

class QrCodeView : public brls::Box {
public:
    /**
     * @brief 构造二维码展示视图
     * @param text 二维码文本
     * @param hintText 底部提示文本
     */
    QrCodeView(const std::string& text, const std::string& hintText = "");

    /**
     * @brief 显示二维码
     * @param text 二维码文本
     * @param hintText 底部提示文本，空字符串时使用默认“请扫描二维码”
     */
    static void show(const std::string& text, const std::string& hintText = "");

    /** @brief 关闭二维码展示 */
    static void close();

    /** @brief 获取二维码卡片作为默认焦点 */
    brls::View* getDefaultFocus() override;

    /** @brief 二维码弹层不使用 AppletFrame */
    brls::AppletFrame* getAppletFrame() override;

    /** @brief 二维码弹层使用半透明背景 */
    bool isTranslucent() override { return true; }

private:
    BRLS_BIND(brls::Box, m_card, "qrCodeView/card");
    BRLS_BIND(QrCode, m_qrcode, "qrCodeView/qrcode");
    BRLS_BIND(brls::Label, m_hint, "qrCodeView/hint");
};
