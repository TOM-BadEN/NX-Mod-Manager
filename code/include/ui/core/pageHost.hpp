/**
 * PageHost - 普通 Page 的内容容器
 *
 * 统一管理 Page 栈、页面所有权、生命周期顺序和当前页面焦点。
 * 支持由调用方选择页面切换动画，不包含路由、业务逻辑或全局外壳状态。
 */

#pragma once

#include "ui/core/page.hpp"
#include <borealis.hpp>
#include <cstddef>
#include <vector>

class PageHost : public brls::Box {
public:
    PageHost();
    ~PageHost() override;

    /**
     * @brief 设置不可被普通 pop 删除的根页面
     * @param page 根页面；成功后所有权交给 PageHost
     * @return 是否设置成功，失败时调用方仍持有 page
     */
    bool setRootPage(Page* page);

    /**
     * @brief 压入并显示一个新页面
     * @param page 新页面；成功后所有权交给 PageHost
     * @param anim 页面切换动画
     * @return 是否切换成功，失败时调用方仍持有 page
     */
    bool pushPage(Page* page, PageAnimType anim = PageAnimType::None);

    /**
     * @brief 移除当前页面并恢复上一页面
     * @param anim 页面切换动画
     * @return 是否返回成功；根页面不会被移除
     */
    bool popPage(PageAnimType anim = PageAnimType::None);

    /**
     * @brief 从栈顶连续移除多个页面并恢复最终页面
     * @param count 要移除的页面数量
     * @param anim 页面切换动画
     * @return 是否返回成功；根页面不会被移除
     */
    bool popPages(std::size_t count, PageAnimType anim = PageAnimType::None);

    /** @brief 当前是否可以返回上一页面 */
    bool canPop() const;

    /** @brief 获取当前页面，没有根页面时返回 nullptr */
    Page* getCurrentPage() const;

    /** @brief 获取当前页面的上一层页面，没有时返回 nullptr */
    Page* getPreviousPage() const;

    /**
     * @brief 判断指定页面是否为当前活动页面
     * @param page 待判断页面
     * @return 页面当前正在显示且 PageHost 可见时返回 true
     */
    bool isPageActive(const Page* page) const;

    /** @brief 获取当前页面栈深度 */
    std::size_t getPageCount() const;

    /** @brief 暂停当前页面，重复调用不会重复触发生命周期 */
    void pauseCurrentPage();

    /** @brief 恢复当前页面，重复调用不会重复触发生命周期 */
    void resumeCurrentPage();

    /** @brief 获取当前页面变化事件 */
    brls::Event<Page*>* getCurrentPageChangedEvent();

    /** @brief 将默认焦点交给当前页面 */
    brls::View* getDefaultFocus() override;

    /** @brief 焦点离开当前页面边界时继续交给父级处理 */
    brls::View* getNextFocus(brls::FocusDirection direction, brls::View* currentView) override;

    /** @brief PageHost 重新显示时恢复当前页面的 View 生命周期 */
    void willAppear(bool resetState = false) override;

    /** @brief PageHost 隐藏时结束当前页面的 View 生命周期 */
    void willDisappear(bool resetState = false) override;

    /** @brief XML View 工厂函数 */
    static brls::View* create();

private:
    /** @brief PageHost 管理的页面生命周期状态 */
    enum class PageState {
        Visible,      // 当前可见页面
        Paused,       // 保留在栈内的暂停页面
        Disappeared,  // 已收到 willDisappear
    };

    /** @brief 页面栈条目 */
    struct PageEntry {
        Page* page;        // PageHost 持有的页面
        PageState state;   // 当前生命周期状态
    };

    std::vector<PageEntry> m_pageStack;        // 根页面位于首项，当前页面位于末项
    brls::Event<Page*> m_currentPageChangedEvent; // 当前页面变化事件
    bool m_hostVisible = false;                 // PageHost 当前是否处于显示状态
    PageAnim m_pageAnim;                        // 当前页面切换动画

    /** @brief 判断页面是否已经在当前栈内 */
    bool containsPage(Page* page) const;

    /** @brief 将页面设置为填充 PageHost 的叠放内容 */
    void preparePage(Page* page);

    /** @brief 获取当前页面条目，没有根页面时返回 nullptr */
    PageEntry* getCurrentEntry();

    /** @brief 获取当前页面条目，没有根页面时返回 nullptr */
    const PageEntry* getCurrentEntry() const;

    /** @brief 通知订阅者当前页面已变化 */
    void notifyCurrentPageChanged();
};
