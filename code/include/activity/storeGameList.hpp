/**
 * StoreGameList - 商店游戏列表页面
 * 
 * 从服务器获取有 MOD 的游戏列表，九宫格展示，支持分页加载
 */

#pragma once
#include <borealis.hpp>
#include <atomic>
#include <memory>
#include "view/recyclingGrid.hpp"
#include "view/myframe.hpp"
#include "core/storeGameManager.hpp"
#include "utils/async.hpp"
#include "view/actionMenu.hpp"
#include "core/gameManager.hpp"

class StoreGameList : public brls::Activity {
public:
    /**
     * @brief 构造商店游戏列表页面
     * @param gameManager 游戏数据管理
     */
    StoreGameList(GameManager& gameManager);

    CONTENT_FROM_XML_RES("activity/storeGameList.xml");

    // 绑定组件
    BRLS_BIND(MyFrame, m_frame, "storeGameList/frame");
    BRLS_BIND(RecyclingGrid, m_grid, "storeGameList/grid");
    BRLS_BIND(brls::Label, m_emptyHint, "storeGameList/emptyHint");

    ~StoreGameList() override;

    /** @brief XML 加载完成后调用 */
    void onContentAvailable() override;

private:
    std::shared_ptr<std::atomic<bool>> m_alive = std::make_shared<std::atomic<bool>>(true);  // 存活标记
    StoreGameManager m_manager;                         // 数据管理
    std::atomic<int> m_focusedIndex{0};                 // 当前焦点索引（后台线程读，主线程写）
    util::AsyncFurture<void> m_loadTask;                // 异步数据加载任务（析构时自动取消）
    util::AsyncFurture<void> m_iconLoader;              // 异步图标加载任务（析构时自动取消）
    util::AsyncFurture<void> m_iconLoader2;             // 第二个图标加载任务（双线程并行下载）
    bool m_loading = false;                             // 是否正在加载
    std::string m_keyword;                              // 当前搜索关键词
    MenuPageConfig m_filterMenu;                        // 筛选菜单
    GameManager& m_gameManager;                         // 游戏数据管理（Home 持有，引用传递）

    /** @brief 初始化网格 + 注册分页回调 */
    void setupGrid();

    /** @brief 初始化搜索 */
    void setupSearch();

    /** @brief 初始化筛选菜单 */
    void setupFilterMenu();

    /** @brief 加载下一页数据 */
    void loadNextPage();

    /** @brief 重置数据 + 重新加载 */
    void reloadData();

    /** @brief 启动异步图标加载任务 */
    void startIconLoader();
};
