/*
    Copyright 2021 XITRIX

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include "borealis/views/cells/cell_detail.hpp"

namespace brls
{

const std::string detailCellXML = R"xml(
    <brls:Box
        width="auto"
        height="@style/brls/sidebar/item_height"
        focusable="true"
        paddingTop="12.5"
        paddingBottom="12.5"
        paddingLeft="@style/brls/listitem/descriptionIndent"
        paddingRight="@style/brls/listitem/descriptionIndent"
        alignItems="center">

        <brls:Label
            id="brls/rediocell/title"
            width="auto"
            height="auto"
            grow="1"
            fontSize="@style/brls/sidebar/item_font_size"
            marginRight="@style/brls/listitem/descriptionIndent" />

        <brls:Label
            id="brls/rediocell/detail"
            horizontalAlign="right"
            maxWidth="300"
            height="auto"
            shrink="0"
            fontSize="@style/brls/sidebar/item_font_size"
            textColor="@theme/brls/list/listItem_value_color"/>

    </brls:Box>
)xml";

DetailCell::DetailCell()
{
    this->inflateFromXMLString(detailCellXML);

    this->registerStringXMLAttribute("title", [this](std::string value)
        { this->title->setText(value); });
}

void DetailCell::setText(std::string title)
{
    this->title->setText(title);
}

void DetailCell::setTextColor(NVGcolor color)
{
    this->title->setTextColor(color);
}

void DetailCell::setDetailText(std::string title)
{
    this->detail->setText(title);
}

void DetailCell::setDetailTextColor(NVGcolor color)
{
    this->detail->setTextColor(color);
}

View* DetailCell::create()
{
    return new DetailCell();
}

} // namespace brls
