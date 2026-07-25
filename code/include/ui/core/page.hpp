/**
 * Page - 全局 UI Shell 中的普通内容页面基类
 *
 * Page 对照 brls::Activity 提供熟悉的生命周期回调，但不负责管理
 * 生命周期调用顺序。PageHost 统一管理 Page 栈、生命周期和焦点。
 *
 * Page 只负责自身内容和页面级业务，不持有 PageHost、全局顶栏、
 * 底栏及其状态；页面切换和全局标题栏控制统一转交给所属 Shell。
 */

#pragma once

#include "ui/anim/pageAnim.hpp"
#include <borealis.hpp>
#include <cstddef>

class Page : public brls::Box {
public:
    Page();
    ~Page() override;

    /**
     * @brief 页面内容可用时调用
     *
     * 对照 brls::Activity::onContentAvailable()。由 PageHost 在页面
     * XML 已加载并可以绑定子 View 后调用一次。
     */
    virtual void onContentAvailable();

    /**
     * @brief 判断页面当前是否处于活动状态
     * @return 页面是 PageHost 中正在显示的当前页面时返回 true
     */
    bool isActive();

    /**
     * @brief 页面即将显示时调用
     * @param resetState 是否重置 View 状态
     *
     * 保留 brls::View 生命周期并向所有子 View 继续传递。
     */
    void willAppear(bool resetState = false) override;

    /** @brief 页面被其他 Page 或临时 Activity 覆盖时调用 */
    virtual void onPause();

    /** @brief 页面从其他 Page 或临时 Activity 返回时调用 */
    virtual void onResume();

    /**
     * @brief 页面即将移除时调用
     * @param resetState 是否重置 View 状态
     *
     * 保留 brls::View 生命周期并向所有子 View 继续传递。
     */
    void willDisappear(bool resetState = false) override;

protected:
    /**
     * @brief 压入并显示一个新页面
     * @param page 新页面；所有权立即交给页面系统
     * @param anim 页面切换动画
     */
    void pushPage(Page* page, PageAnimType anim = PageAnimType::None);

    /**
     * @brief 移除当前页面并返回上一页面
     * @param anim 页面切换动画
     */
    void popPage(PageAnimType anim = PageAnimType::None);

    /**
     * @brief 从栈顶连续移除多个页面并恢复最终页面
     * @param count 要移除的页面数量
     * @param anim 页面切换动画
     */
    void popPages(std::size_t count, PageAnimType anim = PageAnimType::None);

    /**
     * @brief 设置全局标题栏是否读取并显示 FPS
     * @param show 是否读取并显示 FPS
     */
    void setShowFps(bool show);

    /**
     * @brief 设置全局标题栏是否显示并读取内存使用情况
     * @param show 是否显示并读取内存使用情况
     */
    void setShowMem(bool show);
};
