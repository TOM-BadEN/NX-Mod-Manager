/**
 * 主页面（倒计时页面）头文件
 * 
 * Activity 是 Borealis 的页面容器，每个页面对应一个 Activity。
 * 页面的 UI 布局定义在 XML 文件中，通过 CONTENT_FROM_XML_RES 宏绑定。
 */

#pragma once
#include <borealis.hpp>
#include "view/gridPage.hpp"
#include "view/myframe.hpp"

class MainActivity : public brls::Activity {
public:
    CONTENT_FROM_XML_RES("activity/main.xml");
    
    // 绑定组件
    BRLS_BIND(MyFrame, m_frame, "main/frame");
    BRLS_BIND(GridPage, m_gridPage, "main/gridPage");
    BRLS_BIND(brls::Label, m_noModHint, "main/noModHint");
    
    // XML 加载完成后调用
    void onContentAvailable() override;
};
