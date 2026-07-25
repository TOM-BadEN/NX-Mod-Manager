/**
 * 程序入口文件
 *
 * 初始化全局 Shell 和 Home 页面所需的基础模块，并加载 Home 根页面。
 * 其他业务页面将在后续迁移过程中逐项加入。
 */

#include "common/settings.hpp"
#include "common/config.hpp"
#include "core/audio.hpp"
#include "ui/activity/shellActivity.hpp"
#include "ui/page/home.hpp"
#include "ui/page/search.hpp"
#include "ui/theme/theme.hpp"
#include "ui/view/capsuleBadge.hpp"
#include "ui/view/circleButton.hpp"
#include "ui/view/dialog/customDialog.hpp"
#include "ui/view/keyboard.hpp"
#include "ui/view/qrCode.hpp"
#include "ui/view/recyclingGrid.hpp"
#include "utils/gameNacp.hpp"
#include "utils/http.hpp"
#include "utils/pinYinCvt.hpp"
#include <borealis.hpp>
#include <cstdlib>
#include <string>
#include <switch.h>

int main(int argc, char* argv[]) {
    // 保存当前 NRO 路径，供应用更新完成后替换并重启
    config::setNroPath(argv[0]);

    Settings::load();

    // 应用用户选择的语言（必须在框架加载翻译前设置）
    std::string language = Settings::getString("UI", "language", "auto");
    if (language != "auto") brls::Platform::APP_LOCALE_DEFAULT = language;

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

    brls::Application::createWindow("NX Mod Manager");

    // 应用用户选择的主题模式（必须在创建窗口之后）
    std::string theme = Settings::getString("UI", "theme", "auto");
    if (theme == "light") brls::Application::getPlatform()->setThemeVariant(brls::ThemeVariant::LIGHT);
    else if (theme == "dark") brls::Application::getPlatform()->setThemeVariant(brls::ThemeVariant::DARK);

    // 注册应用自定义的深色和浅色主题颜色
    initTheme();

    Audio audio(Settings::getBool("Audio", "muted", false));

    // 开启系统截图和录屏权限
    brls::Application::getPlatform()->forceEnableGamePlayRecording();

    brls::Application::registerXMLView("RecyclingGrid", RecyclingGrid::create);
    brls::Application::registerXMLView("Keyboard", Keyboard::create);
    brls::Application::registerXMLView("ResultButtonGrid", ResultButtonGrid::create);
    brls::Application::registerXMLView("CapsuleBadge", CapsuleBadge::create);
    brls::Application::registerXMLView("CircleButton", CircleButton::create);
    brls::Application::registerXMLView("QrCode", QrCode::create);
    brls::Application::registerXMLView("DialogButton", DialogButton::create);
    brls::Application::registerXMLView("DialogButtonBox", DialogButtonBox::create);

    // 禁用动画
    brls::Application::getStyle().addMetric("brls/animations/show", 0.0f);

    // 禁用框架默认的全局退出（Switch 上为 + 键）
    brls::Application::setGlobalQuit(false);

    // 长时间任务期间禁止息屏，应用退出时恢复系统默认行为
    brls::Application::getPlatform()->disableScreenDimming(true);
    brls::Application::getExitEvent()->subscribe([] {
        brls::Application::getPlatform()->disableScreenDimming(false);
    });

    // 推送全局 Shell Activity，普通页面由其内部 PageHost 管理
    brls::Application::pushActivity(new ShellActivity(new Home()), brls::TransitionAnimation::NONE);

    while (brls::Application::mainLoop()) {
    }

    gameNacp::cleanup();
    http::cleanup();

    return EXIT_SUCCESS;
}
