/**
 * LongPressDialog - 长按确认对话框
 *
 * 基于 CustomDialog(Box*) 容器，不使用其自带按钮。
 * 包含可滚动长文本区域和一个需长按指定时长才能触发的确认按钮。
 * 进度以蓝灰双段分界线呈现，松开重置，到达时长后执行回调。
 */

#pragma once

#include "view/customDialog.hpp"

namespace LongPressDialog {

/**
 * @brief 显示长按确认对话框
 * @param text         显示文本（支持多行，超长自动滚动）
 * @param btnLabel     确认按钮文本
 * @param holdSeconds  需持续按下的时长（秒）
 * @param onConfirm    达到时长后的回调
 * @param bAction      B 键回调，为 nullptr 时禁用 B 键
 */
void show(const std::string& text, const std::string& btnLabel, float holdSeconds, std::function<void()> onConfirm, std::function<void()> bAction = nullptr);

/**
 * @brief 显示长按确认自定义内容对话框
 * @param body         自定义内容容器，所有权交给对话框
 * @param btnLabel     确认按钮文本
 * @param holdSeconds  需持续按下的时长（秒）
 * @param onConfirm    达到时长后的回调
 * @param bAction      B 键回调，为 nullptr 时禁用 B 键
 */
void show(brls::Box* body, const std::string& btnLabel, float holdSeconds, std::function<void()> onConfirm, std::function<void()> bAction = nullptr);

/**
 * @brief 关闭对话框
 * @param cb 关闭后回调（可选）
 */
void close(std::function<void()> cb = [] {});

} // namespace LongPressDialog
