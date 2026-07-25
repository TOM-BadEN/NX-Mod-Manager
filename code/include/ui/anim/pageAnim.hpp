/**
 * PageAnim - 页面切换动画
 *
 * 只负责两个页面 View 之间的视觉过渡，不管理页面栈、页面生命周期、
 * 焦点、输入或页面所有权。
 */

#pragma once

#include <borealis.hpp>
#include <functional>

/** @brief 页面切换动画类型 */
enum class PageAnimType {
    None,            // 无动画
    SlideFromLeft,   // 目标页面从左侧进入，当前页面向右侧移出
    SlideFromRight,  // 目标页面从右侧进入，当前页面向左侧移出
};

/**
 * @brief 页面切换动画播放器
 *
 * PageAnim 只在动画期间临时引用进入和离开的 View，不持有它们。
 * 调用方必须保证两个 View 在动画结束前始终有效。
 */
class PageAnim {
public:
    PageAnim();
    ~PageAnim();

    /**
     * @brief 播放页面切换动画
     * @param entering 进入的目标页面
     * @param leaving 离开的当前页面
     * @param type 动画类型
     * @param width 页面内容区域宽度
     * @param height 页面内容区域高度
     * @param completed 动画正常结束后的回调
     * @return 是否成功开始动画；None 会立即执行完成回调并返回 true
     */
    bool start(brls::View* entering, brls::View* leaving, PageAnimType type, float width, float height, std::function<void()> completed);

    /** @brief 动画当前是否正在运行 */
    bool isRunning();

private:
    static constexpr int ANIM_DURATION = 250; // 页面平移动画持续时间（毫秒）

    brls::Animatable m_progress{0.0f};        // 从 0 到 1 的动画进度
    brls::View* m_entering = nullptr;         // 当前进入的页面，不持有所有权
    brls::View* m_leaving = nullptr;          // 当前离开的页面，不持有所有权
    float m_offsetX = 0.0f;                   // 进入页面的初始横向偏移
    float m_offsetY = 0.0f;                   // 进入页面的初始纵向偏移
    std::function<void()> m_completed;         // 动画正常结束后的回调

    /** @brief 根据当前动画进度更新两个页面的位置 */
    void updateViews();

    /**
     * @brief 结束当前动画并清理临时状态
     * @param finished 动画是否正常播放完成
     */
    void finish(bool finished);

    /** @brief 将两个页面恢复到无平移状态 */
    void resetViews();
};
