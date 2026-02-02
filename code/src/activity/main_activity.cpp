/**
 * 主页面实现文件
 */

#include "activity/main_activity.hpp"
#include "activity/second_activity.hpp"

void MainActivity::onContentAvailable()
{
    btnNewPage->registerClickAction([](brls::View* view) {
        brls::Application::pushActivity(new SecondActivity());
        return true;
    });
}
