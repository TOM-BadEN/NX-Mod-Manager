/**
 * CapsuleBadge - 通用胶囊容器实现
 *
 * 内联布局只定义胶囊的默认外观，内容由使用方的 XML 子元素提供。
 */

#include "ui/view/capsuleBadge.hpp"
#include <string>

const std::string capsuleBadgeXML = R"xml(
    <brls:Box
        width="auto"
        height="auto"
        cornerRadius="12"
        backgroundColor="@theme/app/tagBg"
        paddingTop="6"
        paddingBottom="6"
        paddingLeft="12"
        paddingRight="12"/>
)xml";

CapsuleBadge::CapsuleBadge() {
    this->inflateFromXMLString(capsuleBadgeXML);
}

brls::View* CapsuleBadge::create() {
    return new CapsuleBadge();
}
