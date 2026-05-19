/**
 * StoreGameList - 商店游戏列表页面
 * 
 * 从服务器获取有 MOD 的游戏列表，九宫格展示，支持分页加载
 */

#pragma once
#include <borealis.hpp>
#include <stop_token>
#include "view/recyclingGrid.hpp"
#include "view/myframe.hpp"
#include "core/storeGameManager.hpp"
#include "utils/threadPool.hpp"
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
    std::stop_source m_stopSource;                      // 取消源（页面退出/重载时取消所有任务）
    StoreGameManager m_manager;                         // 数据管理
    int m_focusedIndex = 0;                              // 当前焦点索引
    int m_pendingIcons = 0;                             // 当前正在下载的 icon 数量
    bool m_loading = false;                             // 是否正在加载
    bool m_placeholderMode = false;                     // 是否正在显示假卡片
    std::vector<api::game::GameList> m_placeholderGames; // 假卡片数据
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

    /** @brief 显示假卡片 */
    void showPlaceholders();

    /** @brief 提交 N 个最近的 icon 下载任务到线程池 */
    void submitNextIcons(int count);

    /** @brief 主线程回调：处理分页加载结果 */
    void onPageLoaded(api::game::GameListResult result);

    /** @brief 主线程回调：处理单个 icon 下载结果 */
    void onIconDownloaded(int gameIdx, http::Response response);
};
