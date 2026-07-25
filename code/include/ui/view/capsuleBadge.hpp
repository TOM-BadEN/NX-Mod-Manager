/**
 * CapsuleBadge - 通用胶囊容器
 *
 * 提供胶囊背景、圆角和内边距，具体内容由使用方通过 XML 子元素定义。
 *
 * XML 用法：
 *   <CapsuleBadge marginRight="8">
 *       <brls:Label text="类型" fontSize="16"/>
 *   </CapsuleBadge>
 */

#pragma once

#include <borealis.hpp>

class CapsuleBadge : public brls::Box {
public:
    CapsuleBadge();

    /** @brief 创建胶囊徽章组件 */
    static brls::View* create();
};
