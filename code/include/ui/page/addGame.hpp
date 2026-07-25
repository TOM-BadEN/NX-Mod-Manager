/**
 * AddGame - 添加游戏页面
 *
 * 显示 Switch 设备上已安装的游戏列表，供用户选择并添加到管理器。
 * 页面通过 ShellState 向全局外壳提供标题和当前卡片索引。
 */

#pragma once

#include "core/gameManager.hpp"
#include "ui/core/page.hpp"
#include "ui/core/shellState.hpp"
#include "ui/view/actionMenu.hpp"
#include "ui/view/recyclingGrid.hpp"
#include "utils/fsHelper.hpp"
#include <borealis.hpp>
#include <cstddef>
#include <stop_token>
#include <vector>

class AddGame : public Page, public ShellState {
public:
    /**
     * @brief 创建添加游戏页面并加载 XML 布局
     * @param gameManager 与 Home 共享的游戏数据管理器
     */
    AddGame(GameManager& gameManager);

    /** @brief 停止仍在执行的异步 NACP 加载任务 */
    ~AddGame() override;

    BRLS_BIND(RecyclingGrid, m_grid, "addGame/grid");

    /** @brief XML 加载完成后初始化页面内容和操作 */
    void onContentAvailable() override;

private:
    std::stop_source m_stopSource; // 异步任务取消源
    GameManager& m_gameManager;    // 与 Home 共享的游戏数据管理器
    bool m_sortAsc = true;         // 排序方向
    MenuPageConfig m_addModMenu;   // 添加模组菜单
    int m_focusedIndex = 0;        // 当前焦点索引

    /** @brief 加载虚拟游戏图标并写入 installedGames[0].iconId */
    void loadVirtualGameIcon();

    /** @brief 配置添加模组菜单 */
    void setupMenu();

    /** @brief 初始化网格并注册回调 */
    void setupGridPage();

    /**
     * @brief 设置依赖 NACP 完成的操作是否可用
     * @param available 操作是否可用
     */
    void setNacpActionsAvailable(bool available);

    /** @brief 启动异步 NACP 加载 */
    void startNacpLoader();

    /** @brief 链式提交下一个 NACP 加载任务 */
    void submitNextNacp();

    /**
     * @brief 更新游戏元数据和 UI
     * @param gameIdx 游戏在列表中的索引
     * @param meta 获取到的元数据
     */
    void applyMetadata(size_t gameIdx, const GameMetadata& meta);

    /**
     * @brief 处理游戏卡片点击
     * @param index 点击的游戏索引
     */
    void onGameCardClicked(size_t index);

    /**
     * @brief 执行真实已安装游戏的添加模组流程
     * @param index 游戏在已安装列表中的索引
     * @param mods 中转站中的模组
     */
    void onRealGameCardClicked(size_t index, std::vector<fs::DirEntry> mods);

    /** @brief 虚拟游戏的添加模组流程（需用户输入 TID） */
    void onVirtualGameCardClicked(std::vector<fs::DirEntry> mods);

    /**
     * @brief 创建中转站为空提示内容
     * @return 中转站为空提示 View
     */
    brls::Box* createTransitEmptyBox();

    /**
     * @brief 创建首次进入公告内容
     * @return 首次进入公告 View
     */
    brls::Box* createFirstLaunchBox();

    /** @brief 首次进入时弹出使用说明对话框 */
    void runFirstLaunchDialog();

    /** @brief 切换排序方向 */
    void toggleSort();
};
