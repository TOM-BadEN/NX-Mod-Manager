/**
 * ShellActivity - 全局 UI Shell 的常驻 Activity
 *
 * 只负责承载 AppShell、设置根 Page、处理全局返回，并将 Activity 的
 * 暂停和恢复通知转发给当前 Page。不依赖任何具体业务页面。
 */

#pragma once

#include "ui/core/page.hpp"
#include "ui/view/shell/appShell.hpp"
#include <borealis.hpp>
#include <memory>

class ShellActivity : public brls::Activity {
public:
    /**
     * @brief 创建全局 Shell Activity
     * @param rootPage 根页面；所有权立即交给 ShellActivity
     */
    explicit ShellActivity(Page* rootPage);
    ~ShellActivity() override;

    /** @brief 创建唯一的 AppShell 内容 View */
    brls::View* createContentView() override;

    /** @brief AppShell 可用后，将根页面交给 PageHost 并注册全局返回 */
    void onContentAvailable() override;

    /** @brief 临时 Activity 覆盖时暂停当前 Page */
    void onPause() override;

    /** @brief 临时 Activity 关闭后恢复当前 Page */
    void onResume() override;

private:
    std::unique_ptr<Page> m_rootPage; // 尚未交给 PageHost 的根页面
    AppShell* m_shell = nullptr;      // Activity 内容 View，由 brls::Activity 持有

    /** @brief 处理页面返回或根页面退出确认 */
    bool handleBack();

    /** @brief 显示应用退出确认 */
    void showExitConfirmation();
};
