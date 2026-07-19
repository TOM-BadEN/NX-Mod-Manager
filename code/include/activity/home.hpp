/**
 * Home - 主页面
 * 
 * Activity 是 Borealis 的页面容器，每个页面对应一个 Activity。
 * 页面的 UI 布局定义在 XML 文件中，通过 CONTENT_FROM_XML_RES 宏绑定。
 */

#pragma once
#include <borealis.hpp>
#include <vector>
#include <atomic>
#include "view/recyclingGrid.hpp"
#include "view/myframe.hpp"
#include "core/gameManager.hpp"
#include "utils/async.hpp"
#include "view/actionMenu.hpp"

class Home : public brls::Activity {
public:
    CONTENT_FROM_XML_RES("activity/home.xml");
    
    // 绑定组件
    BRLS_BIND(MyFrame, m_frame, "home/frame");
    BRLS_BIND(RecyclingGrid, m_grid, "home/grid");
    BRLS_BIND(brls::Box, m_noModHint, "home/noModHintBox");
    
    /** @brief XML 加载完成后调用 */
    void onContentAvailable() override;

    /** @brief 页面恢复时刷新焦点和排序 */
    void onResume() override;

private:
    GameManager m_gameManager;                                   // 游戏数据管理
    util::AsyncFurture<void> m_startupUpdateTask;                 // 启动更新检查任务
    util::AsyncFurture<void> m_nacpLoader;                      // 异步 NACP 加载任务
    util::AsyncFurture<void> m_clearTask;                       // 异步清空中转站任务
    util::AsyncFurture<void> m_deleteIconCacheTask;             // 异步删除图标缓存任务
    util::AsyncFurture<void> m_updateTask;                      // 异步下载更新任务
    util::AsyncFurture<void> m_resetTask;                       // 异步重置状态任务
    std::atomic<int> m_focusedIndex{0};                          // 当前焦点索引（后台线程读，主线程写）
    bool m_nacpComplete = false;                                  // NACP 加载是否完成
    bool m_allowForcedUpdate = true;                              // 本次是否允许弹出强制更新
    std::function<void()> m_onNacpComplete;                       // NACP 加载完成后的待执行回调

    /** @brief 显示空列表提示 */
    void showEmptyHint();

    /** @brief 初始化网格 + 注册回调 */
    void setupGridPage();

    /** @brief 请求系统启动指定游戏 */
    void launchGame(uint64_t appId);

    /** @brief 设置依赖 NACP 完成的操作是否可用 */
    void setNacpActionsAvailable(bool available);

    /** @brief 启动异步 NACP 加载 */
    void startNacpLoader();

    /** @brief 启动异步更新检查 */
    void startStartupUpdateCheck();

    /** @brief 切换排序方向 */
    void toggleSort();

    /** @brief 收藏/取消收藏当前游戏 */
    void toggleFavorite();

    /**
     * @brief 应用显示名称（写 JSON + 更新 Cell）
     * @param idx 游戏在列表中的索引
     * @param name 要设置的显示名称
     */
    void applyGameDisplayName(int idx, const std::string& name);

    /** @brief 手动输入游戏显示名称 */
    void manualSetGameDisplayName();

    /**
     * @brief 在线获取游戏显示名称
     * @param token 取消令牌
     * @return 获取结果
     */
    std::any fetchGameDisplayName(std::stop_token token);

    /**
     * @brief 检查在线获取结果并应用
     * @param result 异步获取的结果
     */
    void checkFetchedGameDisplayName(std::any result);

    /** @brief 重置游戏显示名称 */
    void resetGameDisplayName();

    /**
     * @brief 应用 NACP 数据到游戏 + UI
     * @param gameIdx 游戏在列表中的索引
     * @param meta 获取到的元数据
     */
    void applyMetadata(int gameIdx, const GameMetadata& meta);

    /** @brief 打开添加游戏页面（等待加载完成后进入） */
    void openAddGamePage();

    /** @brief 移除当前游戏 */
    void removeGame();

    /** @brief 清空中转站 */
    void clearTransit();

    /** @brief 删除游戏图标缓存 */
    void deleteIconCache();

    /** @brief 创建重置状态说明内容 */
    brls::Box* createResetStateBox();

    /** @brief 重置状态流程 */
    void resetState();

    /** @brief 设置用户昵称 */
    void setNickname();

    /**
     * @brief 检查更新（供 asyncAction 调用）
     * @param token 取消令牌
     * @return 是否有更新
     */
    std::any checkForUpdate(std::stop_token token);

    /** @brief 创建更新详情内容 */
    brls::Box* createUpdateDetailBox();

    /** @brief 展示强制更新弹窗 */
    void showForcedUpdateDialog();

    /** @brief 展示手动更新检查结果 */
    void showManualUpdateResult(std::any result);

    /** @brief 启动下载更新流程 */
    void startUpdateDownload();

    MenuPageConfig m_menu;                                      // 主菜单
    MenuPageConfig m_gameManageMenu;                            // 管理游戏子菜单
    MenuPageConfig m_gameNameMenu;                              // 修改名称子菜单
    MenuPageConfig m_settingsMenu;                              // 功能设置子菜单
    MenuPageConfig m_basicSettingsMenu;                         // 基础设置子菜单
    MenuPageConfig m_advancedSettingsMenu;                      // 高级设置子菜单
    MenuPageConfig m_assistFeaturesMenu;                        // 辅助功能子菜单
    MenuPageConfig m_themeMenu;                                  // 主题颜色子菜单
    MenuPageConfig m_langMenu;                                    // 语言设置子菜单
    MenuPageConfig m_addProjectMenu;                            // 新增项目子菜单
    MenuPageConfig m_storeAddMenu;                               // 商店添加子菜单
    MenuPageConfig m_sortFilterMenu;                             // 排序筛选子菜单
    /** @brief 初始化菜单 */
    void setupMenu();

    /** @brief 初始化"新增项目"子菜单 */
    void setupAddProjectMenu();

    /** @brief 初始化"商店添加"子菜单 */
    void setupStoreAddMenu();

    /** @brief 初始化"修改名称"子菜单 */
    void setupGameNameMenu();

    /** @brief 初始化"管理游戏"子菜单 */
    void setupGameManageMenu();

    /** @brief 初始化"主题颜色"子菜单 */
    void setupThemeMenu();

    /** @brief 初始化"语言设置"子菜单 */
    void setupLangMenu();

    /** @brief 初始化"功能设置"子菜单 */
    void setupSettingsMenu();

    /** @brief 初始化“排序筛选”子菜单 */
    void setupSortFilterMenu();

    /** @brief 创建首屏公告内容 */
    brls::Box* createFirstLaunchBox();

    /** @brief 运行启动对话框序列（首屏公告 → 重复检测） */
    void runStartupDialogs();

    /** @brief 显示重复游戏警告 */
    void showDuplicateWarning(int duplicateCount);
};
