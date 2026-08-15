/**
 * SectionTitle - 通用标题行组件
 *
 * 内置主题色竖线和标题间距，标题内容由调用方通过 XML 或 C++ 添加。
 * 外部布局属性和标题样式由调用方按使用场景设置。
 *
 * XML 用法：
 *   <SectionTitle marginTop="12">
 *       <brls:Label fontSize="22" text="标题"/>
 *   </SectionTitle>
 */

#pragma once

#include <borealis.hpp>

class SectionTitle : public brls::Box {
public:
    SectionTitle();

    /** @brief XML View 工厂函数 */
    static brls::View* create();
};
