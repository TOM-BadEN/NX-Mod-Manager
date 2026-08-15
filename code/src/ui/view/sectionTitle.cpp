/**
 * SectionTitle - 通用标题行组件实现
 *
 * 内联布局只定义标题行和竖线的默认样式，标题内容由调用方提供。
 */

#include "ui/view/sectionTitle.hpp"
#include <string>

const std::string sectionTitleXML = R"xml(
    <brls:Box
        width="auto"
        height="auto"
        axis="row"
        alignItems="center">

        <brls:Box
            width="3"
            height="20"
            marginRight="8"
            cornerRadius="1.5"
            backgroundColor="@theme/app/sectionTitleIndicator"/>

    </brls:Box>
)xml";

SectionTitle::SectionTitle() {
    inflateFromXMLString(sectionTitleXML);
}

brls::View* SectionTitle::create() {
    return new SectionTitle();
}
