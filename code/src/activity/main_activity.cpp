/**
 * 主页面（倒计时页面）实现文件
 * 
 * 这里实现倒计时逻辑：
 * 1. 启动一个每秒触发的定时器
 * 2. 每次触发时更新 Label 显示的数字
 * 3. 倒计时结束后跳转到第二个页面
 */

#include "activity/main_activity.hpp"
#include "activity/second_activity.hpp"

MainActivity::MainActivity()
{
    // 打印日志，方便调试
    brls::Logger::info("MainActivity created");
    
    // 设置定时器的回调函数
    // 这个函数会在每次定时器触发时被调用
    timer.setCallback([this]() {
        if (countdown > 0)
        {
            // 倒计时减 1
            countdown--;
            // 更新 Label 显示的文本
            // countdownLabel 是通过 BRLS_BIND 绑定的 XML 中的控件
            countdownLabel->setText(std::to_string(countdown));
        }
        else
        {
            // 倒计时结束，停止定时器
            timer.stop();
            // 跳转到第二个页面
            // pushActivity 会把新页面压入栈中，按 B 键可以返回
            brls::Application::pushActivity(new SecondActivity());
        }
    });
    
    // 启动定时器，参数是间隔时间（毫秒）
    // 1000ms = 1秒，所以每秒触发一次回调
    timer.start(1000);
}
