/**
 * StoreGameList - 商店游戏列表页面
 *
 * 从服务器获取有 MOD 的游戏列表，九宫格展示，支持分页加载
 */

#pragma once

#include "core/gameManager.hpp"
#include "core/storeGameIconCache.hpp"
#include "core/storeGameManager.hpp"
#include "ui/core/page.hpp"
#include "ui/core/shellState.hpp"
#include "ui/view/contextMenu.hpp"
#include "ui/view/recyclingGrid.hpp"
#include "utils/imageDecoder.hpp"
#include <borealis.hpp>
#include <stop_token>
#include <string>
#include <unordered_set>

class StoreGameList : public Page, public ShellState {
public:
    /**
     * @brief 构造商店游戏列表页面
     * @param gameManager 游戏数据管理
     */
    StoreGameList(GameManager& gameManager);

    BRLS_BIND(RecyclingGrid, m_grid, "storeGameList/grid");
    BRLS_BIND(brls::Label, m_emptyHint, "storeGameList/emptyHint");

    ~StoreGameList() override;

    /** @brief XML 加载完成后调用 */
    void onContentAvailable() override;

private:
    static constexpr const char* TID_PLACEHOLDER = "0000000000000000"; // 尚未选中游戏时显示的 16 位占位 TID

    std::stop_source m_stopSource;                       // 取消源（页面退出/重载时取消所有任务）
    StoreGameManager m_manager;                          // 数据管理
    StoreGameIconCache m_iconFileCache;                  // 本地图标 WebP 和缓存元数据
    std::unordered_set<std::string> m_pendingDownloads;  // 等待网络下载的游戏 TID
    std::unordered_set<std::string> m_checkedIcons;      // 本次页面停留期间已提交校验的 TID
    int m_focusedIndex = 0;                              // 当前焦点索引
    int m_activeDownloads = 0;                           // 当前正在执行的图标下载数量
    bool m_loading = false;                              // 是否正在加载分页
    bool m_iconLoading = false;                          // 本地图标串行流程是否正在运行
    bool m_iconChecking = false;                         // 是否正在执行图标校验
    std::string m_keyword;                               // 当前搜索关键词
    ContextMenuPage m_filterMenu{brls::getStr("page/storeGameList/filterTitle")}; // 筛选菜单
    GameManager& m_gameManager;                          // 游戏数据管理（Home 持有，引用传递）

    /**
     * @brief 设置页面导航标题和 TID 内容标题
     */
    void setHeader();

    /** @brief 初始化网格 + 注册分页回调 */
    void setupGrid();

    /** @brief 初始化搜索 */
    void setupSearch();

    /** @brief 初始化筛选菜单 */
    void setupFilterMenu();

    /** @brief B 键：有搜索词时重置搜索，否则返回主页 */
    bool handleBackOrResetSearch();

    /** @brief 加载下一页数据 */
    void loadNextPage();

    /** @brief 重置数据 + 重新加载 */
    void reloadData();

    /** @brief 显示初始轻量骨架 */
    void showSkeletons();

    /** @brief 启动本地图标串行流程 */
    void startIconLoader();

    /** @brief 按当前焦点提交下一张本地图标 */
    void submitNextIcon();

    /**
     * @brief 将没有本地图片的游戏加入下载队列
     * @param tid 游戏 TID
     */
    void queueIconDownload(std::string tid);

    /**
     * @brief 将纹理失效的卡片重新加入加载流程
     * @param key 纹理缓存 Key
     */
    void queueCardReload(std::string key);

    /**
     * @brief 获取或创建商店游戏图标纹理
     * @param key 纹理缓存 Key
     * @param image 解码后的游戏图标
     * @return 图标纹理 ID
     */
    int loadGameIcon(const std::string& key, const imageDecoder::DecodedImage& image);

    /**
     * @brief 在主线程应用本地图标
     * @param tid 游戏 TID
     * @param image 解码后的游戏图标
     */
    void applyLocalIcon(const std::string& tid, const imageDecoder::DecodedImage& image);

    /**
     * @brief 结束骨架并显示完整卡片
     * @param tid 游戏 TID
     * @param iconId 已取得临时引用的纹理 ID
     */
    void showCard(const std::string& tid, int iconId);

    /** @brief 根据当前网络槽位调度下载和校验任务 */
    void scheduleNetworkTasks();

    /**
     * @brief 提交缺失图标下载任务
     * @param tid 游戏 TID
     */
    void submitIconDownload(std::string tid);

    /**
     * @brief 提交已有图标校验任务
     * @param tid 游戏 TID
     * @param validator 本地图标校验信息
     */
    void submitIconValidation(std::string tid, api::game::IconCacheValidator validator);

    /**
     * @brief 在主线程应用缺失图标下载结果
     * @param tid 游戏 TID
     * @param success 是否正确下载图标
     * @param image 解码后的游戏图标
     */
    void applyDownloadResult(const std::string& tid, bool success, const imageDecoder::DecodedImage& image);

    /** @brief 主线程回调：处理分页加载结果 */
    void onPageLoaded(api::game::GameListResult result);

    /**
     * @brief 处理游戏卡片点击
     * @param index 点击的游戏索引
     */
    void onGameCardClicked(size_t index);
};
