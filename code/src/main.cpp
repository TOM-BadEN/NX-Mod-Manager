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
#include "view/myframe.hpp" // 自定义应用框架

int main(int argc, char* argv[])
{
    // 1. 初始化 Borealis 框架
    //    这会初始化图形、输入、音频等子系统
    if (!brls::Application::init())
    {
        return EXIT_FAILURE;  // 初始化失败则退出
    }

    // 2. 创建窗口，参数是窗口标题
    brls::Application::createWindow("NX Mod Manager");
    
    // 注册自定义视图组件
    brls::Application::registerXMLView("MyFrame", MyFrame::create);
    
    // 启用屏幕调试视图（Switch 上直接显示日志）
    // brls::Application::enableDebuggingView(true);
    
    // 3. 全局主题设置（标题栏和底部栏背景色）
    brls::Theme::getLightTheme().addColor("brls/applet_frame/header", nvgRGB(0, 0, 0));
    brls::Theme::getDarkTheme().addColor("brls/applet_frame/header", nvgRGB(0, 0, 0));
    brls::Theme::getLightTheme().addColor("brls/applet_frame/footer", nvgRGB(0, 0, 0));
    brls::Theme::getDarkTheme().addColor("brls/applet_frame/footer", nvgRGB(0, 0, 0));
    
    // 4. 禁用动画（设为 0 瞬间完成）
    brls::Application::getStyle().addMetric("brls/animations/show", 0.0f);
    
    // 4. 禁用全局退出（按 + 键不会直接退出）
    brls::Application::setGlobalQuit(false);
    
    // 5. 推送主页面到 Activity 栈
    //    Activity 是页面容器，类似 Android 的 Activity
    brls::Application::pushActivity(new MainActivity());

    // 5. 主循环：处理输入、渲染、事件等
    //    返回 false 时退出循环（程序结束）
    while (brls::Application::mainLoop());

    return EXIT_SUCCESS;
}
