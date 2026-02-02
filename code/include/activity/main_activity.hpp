/**
 * 主页面（倒计时页面）头文件
 * 
 * Activity 是 Borealis 的页面容器，每个页面对应一个 Activity。
 * 页面的 UI 布局定义在 XML 文件中，通过 CONTENT_FROM_XML_RES 宏绑定。
 */

#pragma once
#include <borealis.hpp>

class MainActivity : public brls::Activity  // 继承自 brls::Activity
{
public:
    MainActivity();  // 构造函数，在这里初始化逻辑
    
    // 声明此 Activity 的 UI 布局来自 XML 文件
    // 路径相对于 resources/xml/ 目录
    CONTENT_FROM_XML_RES("activity/main.xml");
    
    // 绑定 XML 中 id="countdown" 的 Label 控件到 C++ 变量
    // 这样就可以在代码中通过 countdownLabel 操作这个控件
    BRLS_BIND(brls::Label, countdownLabel, "countdown");
    
private:
    int countdown = 30;           // 倒计时秒数
    brls::RepeatingTimer timer;   // 重复定时器，每隔一段时间触发回调
};
