/**
 * AddGameActivity - 添加游戏页面
 * 显示 Switch 设备上已安装的游戏列表，供用户选择添加
 */

#pragma once

#include <borealis.hpp>
#include "view/recyclingGrid.hpp"
#include "view/myframe.hpp"
#include "core/gameManager.hpp"
#include "utils/threadPool.hpp"
#include "view/actionMenu.hpp"

class AddGameActivity : public brls::Activity {
public:
    CONTENT_FROM_XML_RES("activity/addGameActivity.xml");

    AddGameActivity(GameManager& gameManager);
    ~AddGameActivity() override;

    /** @brief XML 加载完成后调用 */
    void onContentAvailable() override;

private:
    std::stop_source m_stopSource;                               // 异步任务取消源
    GameManager& m_gameManager;                                  // 游戏数据管理
    bool m_sortAsc = true;                                       // 排序方向
    MenuPageConfig m_addModMenu;                                 // 添加模组菜单
    int m_focusedIndex = 0;                                        // 当前焦点索引

    // 绑定组件
    BRLS_BIND(MyFrame, m_frame, "addGame/frame");
    BRLS_BIND(RecyclingGrid, m_grid, "addGame/grid");

    /** @brief 加载虚拟游戏图标并写入 installedGames[0].iconId */
    void loadVirtualGameIcon();

    /** @brief 配置添加模组菜单 */
    void setupMenu();

    /** @brief 初始化网格 + 注册回调 */
    void setupGridPage();

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
     * @brief 游戏卡片点击分发（先检查中转站，再按 index 分叉）
     * @param index 点击的游戏索引
     */
    void onGameCardClicked(size_t index);

    /** @brief 真实已安装游戏的添加模组流程 */
    void onRealGameCardClicked(size_t index, std::vector<fs::DirEntry> mods);

    /** @brief 虚拟游戏的添加模组流程（需用户输入 TID） */
    void onVirtualGameCardClicked(std::vector<fs::DirEntry> mods);

    /** @brief 首次进入时弹出使用说明对话框 */
    void runFirstLaunchDialog();

    /** @brief 切换排序方向 */
    void toggleSort();
};
