/**
 * StoreModList - 商店模组列表页面
 *
 * 从服务器获取指定游戏的模组列表，网格展示，支持分页加载
 */

#pragma once

#include "core/gameManager.hpp"
#include "core/modManager.hpp"
#include "core/storeModManager.hpp"
#include "ui/core/page.hpp"
#include "ui/core/shellState.hpp"
#include "ui/view/contextMenu.hpp"
#include "ui/view/recyclingGrid.hpp"
#include "utils/threadPool.hpp"
#include <borealis.hpp>
#include <optional>
#include <stop_token>
#include <string>

class StoreModList : public Page, public ShellState {
public:
    /**
     * @brief 构造商店模组列表页面
     * @param gameTid 游戏 TID
     * @param gameName 游戏名称
     * @param gameIconKey 游戏图标纹理缓存 Key，空字符串表示不使用
     * @param gameManager 游戏数据管理
     * @param localModManager 本地 ModManager（外部传入或页面补齐；本地游戏不存在时为空）
     * @param gameVersion 本地游戏版本号（"..."表示未安装/未知）
     * @param fromModList 是否从本地 ModList 进入
     */
    StoreModList(std::string gameTid, std::string gameName, std::string gameIconKey, GameManager& gameManager, ModManager* localModManager = nullptr, std::string gameVersion = "...", bool fromModList = false);
    ~StoreModList() override;

    BRLS_BIND(RecyclingGrid, m_grid, "storeModList/grid");
    BRLS_BIND(brls::Label, m_emptyHint, "storeModList/emptyHint");

    /** @brief XML 加载完成后调用 */
    void onContentAvailable() override;

private:
    std::stop_source m_queryStopSource;       // 列表、搜索、筛选和卡片任务取消源
    std::stop_source m_pageStopSource;        // 页面级任务取消源（版本列表和本地 ModManager 准备）
    StoreModManager m_manager;                // 数据管理
    std::string m_gameName;                   // 页面标题（游戏名）
    std::string m_gameIconKey;                // 游戏图标纹理缓存 Key
    int m_focusedIndex = 0;                   // 当前焦点索引
    bool m_loading = false;                   // 是否正在加载分页
    bool m_cardLoading = false;               // 卡片逐帧加载流程是否正在运行

    // 筛选菜单
    bool m_versionsLoaded = false;            // 版本列表是否已加载
    bool m_filterDirty = false;               // 筛选条件是否有变化（延迟刷新用）
    ContextMenuPage m_filterMenu{brls::getStr("page/storeModList/filterMenuTitle")};   // X 键筛选菜单
    ContextMenuPage m_sortMenu{brls::getStr("page/storeModList/sortMenuTitle")};       // 排序子菜单
    ContextMenuPage m_modTypeMenu{brls::getStr("page/storeModList/typeMenuTitle")};    // 模组类型子菜单
    ContextMenuPage m_versionMenu{brls::getStr("page/storeModList/versionMenuTitle")}; // 游戏版本子菜单
    GameManager& m_gameManager;               // 游戏数据管理（Home 持有，引用传递）
    ModManager* m_localModManager = nullptr;   // 本地 ModManager（外部传入或页面补齐；本地游戏不存在时为空）
    std::optional<ModManager> m_pageModManager; // 页面自己创建的本地 ModManager
    std::string m_gameVersion;                // 本地游戏版本号（"..."表示未安装/未知）
    bool m_fromModList = false;               // 是否从本地 ModList 进入
    bool m_localManagerReady = false;         // 本地 ModManager 首次准备是否完成
    std::optional<api::mod::ModListResult> m_pendingFirstPage; // 等待首次加载条件的第一页结果
    WaitableTask m_localManagerTask;           // 本地 ModManager 准备任务（析构自动等待）

    /** @brief 获取当前普通标题状态 */
    TitleState getHeaderTitle() const;

    /** @brief 设置页面标题 */
    void setHeader();

    /** @brief 单独更新普通标题 */
    void setHeaderTitle();

    /** @brief 设置导航操作 */
    void setupNavigationActions();

    /** @brief 初始化网格 + 注册分页回调 */
    void setupGrid();

    /** @brief 设置搜索和筛选操作是否可用 */
    void setQueryActionsAvailable(bool available);

    /** @brief B 键：有搜索词时重置搜索，否则返回上一页 */
    bool handleBackOrResetSearch();

    /** @brief 加载下一页数据 */
    void loadNextPage();

    /** @brief 分页加载完成回调（主线程） */
    void onPageLoaded(api::mod::ModListResult result, std::stop_token token);

    /** @brief 初始化搜索 */
    void setupSearch();

    /** @brief 初始化筛选菜单（X 键） */
    void setupFilterMenu();

    /** @brief 异步加载版本列表并构建版本子菜单 */
    void loadVersionMenu();

    /** @brief 重置数据 + 重新加载（筛选/搜索变化后） */
    void reloadData();

    /** @brief 显示初始轻量骨架 */
    void showSkeletons();

    /** @brief 尝试完成首次页面加载 */
    void tryFinishInitialLoad();

    /** @brief 启动卡片逐帧加载流程 */
    void startCardLoader();

    /** @brief 按当前焦点提交下一张卡片 */
    void submitNextCard();

    /**
     * @brief 结束骨架并显示完整卡片
     * @param index 模组索引
     */
    void showCard(size_t index);

    /** @brief 启动本地 ModManager 首次准备任务 */
    void startLocalModManagerTask();

    /** @brief 尝试准备本地 ModManager（本地游戏不存在时保持为空） */
    void prepareLocalModManager();

    /** @brief 本地 ModManager 首次准备完成回调（主线程） */
    void onLocalModManagerReady();

    /** @brief 是否从本地 ModList 页面进入 */
    bool isFromModList() const;

    /**
     * @brief 给商店模组填充本地下载和更新状态
     * @param mod 商店模组列表项
     */
    void applyLocalState(api::mod::ModList& mod);

    /**
     * @brief 打开模组详情页
     * @param index 列表项索引
     */
    void openDetail(size_t index);
};
