/**
 * navigate - 页面导航封装
 *
 * 统一使用 TransitionAnimation::NONE 切换页面，
 * 避免 FADE 过渡期间新旧页面同时绘制导致顶部状态栏闪烁。
 */

#pragma once

#include <borealis.hpp>

namespace pageNav {

enum class Anim { NONE, SLIDE_LEFT, SLIDE_RIGHT, SLIDE_DOWN, SLIDE_UP };

/**
 * @brief 压入新页面
 * @param activity 目标页面
 * @param anim 过渡动画
 */
void push(brls::Activity* activity, Anim anim = Anim::NONE);

/** @brief 弹出当前页面
 *  @param anim 过渡动画
 */
void pop(Anim anim = Anim::NONE);

/**
 * @brief 连续弹出 n 层页面，中间层无动画，最后一层使用指定动画
 * @param n 弹出层数
 * @param anim 最后一层的过渡动画
 */
void popN(int n, Anim anim = Anim::NONE);

} // namespace pageNav
