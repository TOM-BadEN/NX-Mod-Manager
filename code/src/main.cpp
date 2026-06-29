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
#include <switch.h>
#include "activity/home.hpp"          // 主页面
#include "view/myframe.hpp"           // 自定义应用框架
#include "view/recyclingGrid.hpp"     // 回收网格组件
#include "view/keyboard.hpp"          // 键盘组件
#include "view/customDialog.hpp"     // 对话框（含 DialogButton）
#include "activity/searchActivity.hpp" // 搜索页面（含 ResultButtonGrid）
#include "theme/theme.hpp"              // 主题初始化
#include "core/audio.hpp"               // 音效模块
#include "utils/pinYinCvt.hpp"          // 拼音工具
#include "view/circleButton.hpp"        // 圆形图标按钮组件
#include "view/capsuleBadge.hpp"        // 胶囊徽章组件
#include "view/qrCode.hpp"              // 二维码组件
#include "utils/http.hpp"                 // HTTP 模块（含 libcurl 全局生命周期管理）
#include "utils/gameNacp.hpp"              // NACP 数据获取工具
#include "common/settings.hpp"              // 全局配置
#include "common/config.hpp"                // 常量配置
#include "core/appUpdater.hpp"              // 应用更新
#include "core/device.hpp"                  // 设备信息
#include "utils/threadPool.hpp"             // 线程池

int main(int argc, char* argv[]) {

    // 保存当前 NRO 路径（用于重启）
    config::setNroPath(argv[0]);

    // 加载全局配置（需在框架初始化前，以便设置语言）
    Settings::load();

    // 应用语言设置（必须在 Application::init() 之前）
    std::string lang = Settings::getString("UI", "language", "auto");
    if (lang != "auto") brls::Platform::APP_LOCALE_DEFAULT = lang;

    // 初始化框架
    if (!brls::Application::init()) return EXIT_FAILURE;

    // 初始化 HTTP 模块
    http::init();

    // 初始化 NACP 服务
    gameNacp::init();

#ifdef NXLINK
    nxlinkStdio();
#endif

    // 初始化拼音引擎（加载字典）
    pinYinCvt::init();

    // 创建窗口
    brls::Application::createWindow("NX Mod Manager");

    // 应用主题设置（必须在 createWindow 之后）
    std::string theme = Settings::getString("UI", "theme", "auto");
    if (theme == "light") brls::Application::getPlatform()->setThemeVariant(brls::ThemeVariant::LIGHT);
    else if (theme == "dark") brls::Application::getPlatform()->setThemeVariant(brls::ThemeVariant::DARK);

    // 初始化音效模块
    bool soundMuted = Settings::getBool("Audio", "muted", false);
    Audio audio(soundMuted);

    // 开启截图/录屏权限
    brls::Application::getPlatform()->forceEnableGamePlayRecording();
    
    // 注册自定义视图组件
    brls::Application::registerXMLView("MyFrame", MyFrame::create);
    brls::Application::registerXMLView("RecyclingGrid", RecyclingGrid::create);
    brls::Application::registerXMLView("Keyboard", Keyboard::create);
    brls::Application::registerXMLView("DialogButton", DialogButton::create);
    brls::Application::registerXMLView("DialogButtonBox", DialogButtonBox::create);
    brls::Application::registerXMLView("ResultButtonGrid", ResultButtonGrid::create);
    brls::Application::registerXMLView("CircleButton", CircleButton::create);
    brls::Application::registerXMLView("CapsuleBadge", CapsuleBadge::create);
    brls::Application::registerXMLView("QrCode", QrCode::create);
    
    // 初始化主题颜色
    initTheme();
    
    // 启用屏幕调试视图（Switch 上直接显示日志）
    // brls::Application::enableDebuggingView(true);
    
    // 禁用动画
    brls::Application::getStyle().addMetric("brls/animations/show", 0.0f);

    // 禁用全局退出（框架默认是按+键退出）
    brls::Application::setGlobalQuit(false);
    
    // 禁止息屏（避免长时间任务被中断），退出时恢复
    brls::Application::getPlatform()->disableScreenDimming(true);
    brls::Application::getExitEvent()->subscribe([] {
        brls::Application::getPlatform()->disableScreenDimming(false);
    });

    // 异步检查nro更新
    if (deviceInfo::Network::isAvailable()) {
        auto& updater = AppUpdater::instance();
        ThreadPool::instance().submit([](std::stop_token token) {
            auto& updater = AppUpdater::instance();
            updater.check(token, APP_VERSION);
            if (token.stop_requested()) return;
            if (!updater.hasUpdate()) return;

            std::string msg = brls::getStr("main/updateFound", updater.tagName());
            brls::sync([msg = std::move(msg)] {
                brls::Application::getStyle().addMetric("brls/animations/notification_timeout", 6000.0f);
                brls::Application::notify(msg);
                brls::Application::getStyle().addMetric("brls/animations/notification_timeout", 4000.0f);
            });
        }, updater.token());
    }

    // 推送主页面
    brls::Application::pushActivity(new Home());

    // 主循环
    while (brls::Application::mainLoop());

    gameNacp::cleanup();
    http::cleanup();

    return EXIT_SUCCESS;
}
