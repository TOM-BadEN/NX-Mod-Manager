/**
 * CapsuleBadge - 胶囊徽章组件
 *
 * 圆角胶囊形状，tagBg 背景，内嵌 Label 文本。
 * 参考 brls::Button 设计，通过 forwardXMLAttribute 转发属性到内部 Label。
 *
 * XML 用法：
 *   <CapsuleBadge text="类型" marginRight="8"/>
 *   <CapsuleBadge text="v1.0" fontSize="16" paddingTop="4" paddingBottom="4"/>
 *
 * 可覆盖属性：
 *   - text / fontSize / singleLine / textColor — 转发到内部 Label
 *   - padding / margin / cornerRadius / backgroundColor — 内置属性直接覆盖
 */

#pragma once

#include <borealis.hpp>

class CapsuleBadge : public brls::Box
{
  public:
    CapsuleBadge();

    /**
     * @brief 设置文本
     * @param text 文本内容
     */
    void setText(std::string text);

    /** @brief 获取文本 */
    std::string getText();

    /** @brief 获取内部 Label */
    brls::Label* getLabel();

    static brls::View* create();

  private:
    BRLS_BIND(brls::Label, m_label, "capsuleBadge/label");
};
