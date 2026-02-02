/**
 * 第二个页面头文件
 * 
 * 这是一个最简单的 Activity 示例：
 * - 只有 CONTENT_FROM_XML_RES 绑定 XML 布局
 * - 没有额外的逻辑代码
 * - 没有绑定任何控件
 */

#pragma once
#include <borealis.hpp>

class SecondActivity : public brls::Activity
{
public:
    // 绑定 XML 布局文件
    // 如果页面只需要显示静态内容，这一行就够了
    CONTENT_FROM_XML_RES("activity/second.xml");
};
