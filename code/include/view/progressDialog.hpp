/**
 * ProgressDialog - 进度对话框
 *
 * 封装 progressDialog.xml 布局，基于 CustomDialog 显示。
 * 全部静态 API：show() 创建并显示，setter 更新进度。
 * 替换式机制自动处理：新建任何 CustomDialog 会替换当前进度对话框。
 */

#pragma once

#include "view/customDialog.hpp"

namespace ProgressDialog {

/**
 * @brief 创建并显示进度对话框
 * @param title   标题文本
 * @param buttons 按钮列表（可选，无按钮时传空或省略）
 * @param bAction B 键回调（可选）
 */
void show(const std::string& title,
    std::vector<CustomDialog::ButtonConfig> buttons = {},
    std::function<void()> bAction = nullptr);

/**
 * @brief 更新标题文本
 * @param text 标题
 */
void setTitle(const std::string& text);

/**
 * @brief 更新左侧文本
 * @param text 文本
 */
void setLeftText(const std::string& text);

/**
 * @brief 更新右侧文本
 * @param text 文本
 */
void setRightText(const std::string& text);

/**
 * @brief 更新主进度条
 * @param percent 百分比
 */
void setMainProgress(float percent);

/**
 * @brief 更新主进度条颜色
 * @param color 颜色
 */
void setMainProgressColor(NVGcolor color);

/** @brief 恢复主进度条默认颜色 */
void resetMainProgressColor();

/**
 * @brief 更新副进度条
 * @param written 已写入字节数
 * @param total 总字节数
 */
void setSubProgress(int64_t written, int64_t total);

/** @brief 隐藏副进度条 */
void hideSubProgress();

/** @brief 隐藏所有按钮并禁用 B 键 */
void hideButtons();

/**
 * @brief 关闭对话框
 * @param cb 关闭后回调
 */
void close(std::function<void()> cb = [] {});

} // namespace ProgressDialog
