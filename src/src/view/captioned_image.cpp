/*
    Copyright 2020-2021 natinusala

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

#include "view/captioned_image.hpp"

CaptionedImage::CaptionedImage()
{
    // 从 XML 文件加载布局
    // XML 中的顶层 Box 对应当前实例，所有 XML 子元素都会添加为子视图
    this->inflateFromXMLRes("xml/views/captioned_image.xml");

    // 标签默认隐藏，获得焦点时才显示
    this->label->hide([] {});

    // 转发 XML 属性到子视图
    this->forwardXMLAttribute("scalingType", this->image);
    this->forwardXMLAttribute("image", this->image);
    this->forwardXMLAttribute("focusUp", this->image);
    this->forwardXMLAttribute("focusRight", this->image);
    this->forwardXMLAttribute("focusDown", this->image);
    this->forwardXMLAttribute("focusLeft", this->image);
    this->forwardXMLAttribute("imageWidth", this->image, "width");
    this->forwardXMLAttribute("imageHeight", this->image, "height");

    this->forwardXMLAttribute("caption", this->label, "text");

    this->addGestureRecognizer(new brls::TapGestureRecognizer(this, brls::TapGestureConfig(false, brls::SOUND_NONE, brls::SOUND_NONE, brls::SOUND_NONE)));
}

void CaptionedImage::onChildFocusGained(brls::View* directChild, brls::View* focusedView)
{
    // 子视图获得焦点时调用 (这里是 Image)

    Box::onChildFocusGained(directChild, focusedView);

    this->label->show([] {});
}

void CaptionedImage::onChildFocusLost(brls::View* directChild, brls::View* focusedView)
{
    // 子视图失去焦点时调用 (这里是 Image)

    Box::onChildFocusLost(directChild, focusedView);

    this->label->hide([] {});
}

brls::View* CaptionedImage::create()
{
    // XML 引擎调用此方法创建 CaptionedImage
    return new CaptionedImage();
}
