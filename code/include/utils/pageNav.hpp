/**
 * navigate - 页面导航封装
 *
 * 统一使用 TransitionAnimation::NONE 切换页面，
 * 避免 FADE 过渡期间新旧页面同时绘制导致顶部状态栏闪烁。
 */

#pragma once

#include <borealis.hpp>

namespace pageNav {

/**
 * @brief 切换页面（无动画）
 * @param activity 目标页面
 */
inline void push(brls::Activity* activity) {
    brls::Application::pushActivity(activity, brls::TransitionAnimation::NONE);
}

} // namespace pageNav
