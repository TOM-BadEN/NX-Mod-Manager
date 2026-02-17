/**
 * 程序入口文件
 * 
 * Borealis 应用的基本流程：
 * 1. 初始化框架
 * 2. 创建窗口
 * 3. 推送第一个 Activity（页面）
 * 4. 进入主循环
 */

#include <borealis.hpp>              // Borealis 框架主头文件
#include "activity/main_activity.hpp" // 我们的主页面
#include "view/myframe.hpp"           // 自定义应用框架
#include "view/gameCard.hpp"          // 游戏卡片组件
#include "view/recyclingGrid.hpp"     // 回收网格组件
#include "view/modCard.hpp"           // Mod 卡片组件
#include "view/modList.hpp"           // Mod 列表组件
#include "view/modDetail.hpp"         // Mod 详情组件（占位）
#include "theme/theme.hpp"              // 主题初始化
#include "utils/pinYinCvt.hpp"          // 拼音工具
#ifdef NXLINK
#include <switch.h>
#endif

int main(int argc, char* argv[]) {
    
    if (!brls::Application::init()) return EXIT_FAILURE;

#ifdef NXLINK
    nxlinkStdio();
#endif

    // 初始化拼音引擎（加载字典）
    pinYinCvt::init();

    // 创建窗口
    brls::Application::createWindow("NX Mod Manager");
    
    // 注册自定义视图组件
    brls::Application::registerXMLView("MyFrame", MyFrame::create);
    brls::Application::registerXMLView("GameCard", GameCard::create);
    brls::Application::registerXMLView("RecyclingGrid", RecyclingGrid::create);
    brls::Application::registerXMLView("ModCard", ModCard::create);
    brls::Application::registerXMLView("ModList", ModList::create);
    brls::Application::registerXMLView("ModDetail", ModDetail::create);
    
    // 初始化主题颜色
    initTheme();
    
    // 启用屏幕调试视图（Switch 上直接显示日志）
    // brls::Application::enableDebuggingView(true);
    
    // 禁用动画
    brls::Application::getStyle().addMetric("brls/animations/show", 0.0f);
    
    // 禁用全局退出
    brls::Application::setGlobalQuit(false);
    
    // 推送主页面
    brls::Application::pushActivity(new MainActivity());

    // 主循环
    while (brls::Application::mainLoop());

    return EXIT_SUCCESS;
}
