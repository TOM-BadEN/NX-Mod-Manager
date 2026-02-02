//
//  dropdown.cpp
//  borealis
//
//  Created by Даниил Виноградов on 14.05.2021.
//

#include "borealis/views/dropdown.hpp"

#include "borealis/core/application.hpp"
#include "borealis/views/cells/cell_radio.hpp"

namespace brls
{

const std::string dropdownFrameXML = R"xml(
<brls:Box
    width="1280"
    height="auto"
    axis="column">

    <brls:AppletFrame
        id="brls/dropdown/applet"
        width="auto"
        height="auto"
        grow="1"
        headerHidden="true"
        footerHidden="true"
        backgroundColor="@theme/brls/backdrop">

        <brls:Box
            width="auto"
            height="auto"
            grow="1"
            axis="column"
            justifyContent="flexEnd">

            <brls:Box
                id="brls/dropdown/content"
                width="auto"
                height="330"
                axis="column"
                backgroundColor="@theme/brls/background">

                <brls:Box
                    id="brls/dropdown/header"
                    width="auto"
                    height="@style/brls/dropdown/header_height"
                    axis="row"
                    paddingTop="@style/brls/applet_frame/header_padding_top_bottom"
                    paddingBottom="@style/brls/applet_frame/header_padding_top_bottom"
                    paddingLeft="@style/brls/applet_frame/header_padding_sides"
                    paddingRight="@style/brls/applet_frame/header_padding_sides"
                    marginLeft="@style/brls/applet_frame/padding_sides"
                    marginRight="@style/brls/applet_frame/padding_sides"
                    lineColor="@theme/brls/applet_frame/separator"
                    lineBottom="1px">

                    <brls:Label
                        id="brls/dropdown/title_label"
                        width="auto"
                        height="auto"
                        marginTop="@style/brls/applet_frame/header_title_top_offset"
                        fontSize="@style/brls/dropdown/header_title_font_size" />

                </brls:Box>
            
                <brls:Box
                    width="auto"
                    height="auto"
                    axis="row"
                    grow="1"
                    justifyContent="center"
                    alignItems="stretch">
                    
                    <brls:RecyclerFrame
                        id="brls/dropdown/recycler"
                        width="100%"
                        height="auto"
                        paddingTop="@style/brls/dropdown/listPadding"
                        paddingRight="@style/brls/dropdown/listPaddingSides"
                        paddingBottom="@style/brls/dropdown/listPadding"
                        paddingLeft="@style/brls/dropdown/listPaddingSides"/>

                </brls:Box>

            </brls:Box>

        </brls:Box>

    </brls:AppletFrame>
    <brls:BottomBar
        backgroundColor="@theme/brls/background"/>
</brls:Box>
)xml";

float min(float a, float b)
{
    if (a < b)
        return a;
    return b;
}

Dropdown::Dropdown(std::string title, std::vector<std::string> values, ValueSelectedEvent::Callback cb, int selected, ValueSelectedEvent::Callback dismissCb)
    : cb(cb)
    , dismissCb(dismissCb)
    , values(values)
    , selected(selected)
{
    this->inflateFromXMLString(dropdownFrameXML);
    this->title->setText(title);

    recycler->estimatedRowHeight = Application::getStyle()["brls/dropdown/listItemHeight"];
    recycler->registerCell("Cell", []()
        {
        RadioCell* cell = new RadioCell();
        cell->setHeight(Application::getStyle()["brls/dropdown/listItemHeight"]);
        cell->title->setFontSize(Application::getStyle()["brls/dropdown/listItemTextSize"]);
        return cell; });
    recycler->setDefaultCellFocus(IndexPath(0, selected));
    recycler->setDataSource(this, false);

    Style style = Application::getStyle();

    float height = numberOfRows(recycler, 0) * style["brls/dropdown/listItemHeight"]
        + header->getHeight()
        + style["brls/dropdown/listPadding"] // top
        + style["brls/dropdown/listPadding"] // bottom
        ;

    content->setHeight(min(height, Application::contentHeight * 0.73f));
}

int Dropdown::numberOfRows(RecyclerFrame* recycler, int section)
{
    return (int)values.size();
}

RecyclerCell* Dropdown::cellForRow(RecyclerFrame* recycler, IndexPath index)
{
    RadioCell* cell = (RadioCell*)recycler->dequeueReusableCell("Cell");
    cell->title->setText(values[index.row]);
    cell->setSelected(index.row == selected);
    return cell;
}

void Dropdown::didSelectRowAt(RecyclerFrame* recycler, IndexPath index)
{
    this->cb(index.row);
    Application::popActivity(TransitionAnimation::FADE, [this, index]
        { this->dismissCb(index.row); });
}

AppletFrame* Dropdown::getAppletFrame()
{
    return applet;
}

void Dropdown::show(std::function<void(void)> cb, bool animate, float animationDuration)
{
    if (animate)
    {
        content->setTranslationY(30.0f);

        showOffset.stop();
        showOffset.reset(30.0f);
        showOffset.addStep(0, animationDuration, EasingFunction::quadraticOut);
        showOffset.setTickCallback([this]
            { this->offsetTick(); });
        showOffset.start();
    }

    Box::show(cb, animate, animationDuration);

    if (animate)
    {
        alpha.stop();
        alpha.reset(1);

        applet->alpha.stop();
        applet->alpha.reset(0);
        applet->alpha.addStep(1, animationDuration, EasingFunction::quadraticOut);
        applet->alpha.start();
    }
}

void Dropdown::hide(std::function<void(void)> cb, bool animated, float animationDuration)
{

    if (animated)
    {
        alpha.stop();
        alpha.reset(0);

        applet->alpha.stop();
        applet->alpha.reset(1);
        applet->alpha.addStep(0, animationDuration, EasingFunction::quadraticOut);
        applet->alpha.start();
    }

    Box::hide(cb, animated, animationDuration);
}

View* Dropdown::getParentNavigationDecision(View* from, View* newFocus, FocusDirection direction)
{
    View* result = Box::getParentNavigationDecision(from, newFocus, direction);

    RecyclerCell* cell = dynamic_cast<RecyclerCell*>(result);
    if (cell && cell != from)
    {
        cellFocusDidChangeEvent.fire(cell);
    }

    return result;
}

float Dropdown::getShowAnimationDuration(TransitionAnimation animation)
{
    return View::getShowAnimationDuration(animation) / 2;
}

void Dropdown::offsetTick()
{
    content->setTranslationY(showOffset);
}

} // namespace brls
