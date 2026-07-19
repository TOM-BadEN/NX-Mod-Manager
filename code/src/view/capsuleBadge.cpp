/**
 * CapsuleBadge - 胶囊徽章组件实现
 *
 * 参考 brls::Button 设计，使用 inflateFromXMLString 内联布局，
 * forwardXMLAttribute 转发属性到内部 Label。
 */

#include "view/capsuleBadge.hpp"

const std::string capsuleBadgeXML = R"xml(
    <brls:Box
        width="auto"
        height="auto"
        cornerRadius="12"
        backgroundColor="@theme/app/tagBg"
        paddingTop="6"
        paddingBottom="6"
        paddingLeft="12"
        paddingRight="12">

        <brls:Label
            id="capsuleBadge/label"
            width="auto"
            height="auto"
            fontSize="18"
            singleLine="true"/>

    </brls:Box>
)xml";

CapsuleBadge::CapsuleBadge()
{
    this->inflateFromXMLString(capsuleBadgeXML);

    // 转发 XML 属性到内部 Label
    this->forwardXMLAttribute("text", this->m_label);
    this->forwardXMLAttribute("fontSize", this->m_label);
    this->forwardXMLAttribute("singleLine", this->m_label);
    this->forwardXMLAttribute("textColor", this->m_label);
}

void CapsuleBadge::setText(std::string text)
{
    this->m_label->setText(text);
}

std::string CapsuleBadge::getText()
{
    return this->m_label->getFullText();
}

brls::Label* CapsuleBadge::getLabel()
{
    return this->m_label;
}

brls::View* CapsuleBadge::create()
{
    return new CapsuleBadge();
}
