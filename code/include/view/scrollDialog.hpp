/**
 * ScrollDialog - 可滚动长文本对话框
 *
 * 封装 scrollDialog.xml 布局，基于 CustomDialog 显示。
 * 全部静态 API：show() 创建并显示，上下键滚动内容，两个并排按钮。
 */

#pragma once

#include "view/customDialog.hpp"

namespace ScrollDialog {

/**
 * @brief 显示可滚动长文本对话框
 * @param text    显示文本（支持多行，超长自动滚动）
 * @param btn1    左按钮文本
 * @param btn1Cb  左按钮回调
 * @param btn2    右按钮文本
 * @param btn2Cb  右按钮回调
 */
void show(const std::string& text,
    const std::string& btn1, std::function<void()> btn1Cb,
    const std::string& btn2, std::function<void()> btn2Cb,
    std::function<void()> bAction = nullptr);

/**
 * @brief 关闭对话框
 * @param cb 关闭后回调（可选）
 */
void close(std::function<void()> cb = [] {});

} // namespace ScrollDialog
