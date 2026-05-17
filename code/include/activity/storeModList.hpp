/**
 * StoreModList - 商店模组列表页面
 *
 * 从服务器获取指定游戏的模组列表，网格展示，支持分页加载
 */

#pragma once
#include <borealis.hpp>
#include "view/recyclingGrid.hpp"
#include "view/myframe.hpp"
#include "view/actionMenu.hpp"
#include "core/storeModManager.hpp"
#include "utils/threadPool.hpp"
#include "core/gameManager.hpp"
#include "core/modManager.hpp"

class StoreModList : public brls::Activity {
public:
    /**
     * @brief 构造商店模组列表页面
     * @param gameTid 游戏 TID
     * @param gameName 游戏名称
     * @param gameManager 游戏数据管理
     * @param localModManager 本地 ModManager（从 ModList 进入时非空）
     * @param gameVersion 本地游戏版本号（"..."表示未安装/未知）
     */
    StoreModList(std::string gameTid, std::string gameName, GameManager& gameManager, ModManager* localModManager = nullptr, std::string gameVersion = "...");
    ~StoreModList();

    CONTENT_FROM_XML_RES("activity/storeModList.xml");

    // 绑定组件
    BRLS_BIND(MyFrame, m_frame, "storeModList/frame");
    BRLS_BIND(RecyclingGrid, m_grid, "storeModList/grid");
    BRLS_BIND(brls::Label, m_emptyHint, "storeModList/emptyHint");

    /** @brief XML 加载完成后调用 */
    void onContentAvailable() override;

private:
    std::stop_source m_stopSource;                     // 异步任务取消源
    StoreModManager m_manager;                          // 数据管理
    std::string m_gameName;                             // 页面标题（游戏名）
    bool m_loading = false;                             // 是否正在加载

    // 筛选菜单
    bool m_versionsLoaded = false;                      // 版本列表是否已加载
    bool m_filterDirty = false;                         // 筛选条件是否有变化（延迟刷新用）
    MenuPageConfig m_filterMenu;                        // Y 键筛选菜单
    MenuPageConfig m_sortMenu;                          // 排序子菜单
    MenuPageConfig m_modTypeMenu;                       // 模组类型子菜单
    MenuPageConfig m_versionMenu;                       // 游戏版本子菜单
    GameManager& m_gameManager;                         // 游戏数据管理（Home 持有，引用传递）
    ModManager* m_localModManager = nullptr;             // 本地 ModManager（从 ModList 进入时非空）
    std::string m_gameVersion;                            // 本地游戏版本号（"..."表示未安装/未知）

    /** @brief 初始化网格 + 注册分页回调 */
    void setupGrid();

    /** @brief 加载下一页数据 */
    void loadNextPage();

    /** @brief 分页加载完成回调（主线程） */
    void onPageLoaded(api::mod::ModListResult result, std::stop_token token);

    /** @brief 初始化搜索 */
    void setupSearch();

    /** @brief 初始化筛选菜单（Y 键） */
    void setupFilterMenu();

    /** @brief 异步加载版本列表并构建版本子菜单 */
    void loadVersionMenu();

    /** @brief 重置数据 + 重新加载（筛选/搜索变化后） */
    void reloadData();
};
