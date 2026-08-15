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
#include "ui/view/contextMenu.hpp"
#include "ui/view/recyclingGrid.hpp"
#include "utils/fsHelper.hpp"
#include "utils/imageDecoder.hpp"
#include <borealis.hpp>
#include <cstddef>
#include <cstdint>
#include <stop_token>
#include <string>
#include <vector>

class AddGame : public Page, public ShellState {
public:
    /**
     * @brief 创建添加游戏页面并加载 XML 布局
     * @param gameManager 与 Home 共享的游戏数据管理器
     */
    AddGame(GameManager& gameManager);

    /** @brief 停止仍在执行的后台加载和逐帧任务 */
    ~AddGame() override;

    BRLS_BIND(RecyclingGrid, m_grid, "addGame/grid");

    /** @brief XML 加载完成后初始化页面内容和操作 */
    void onContentAvailable() override;

private:
    std::stop_source m_stopSource;      // 异步任务取消源
    GameManager& m_gameManager;         // 与 Home 共享的游戏数据管理器
    bool m_sortAsc = true;              // 排序方向
    bool m_searchReady = false;         // 首次 NACP 是否全部加载完成
    ContextMenuPage m_addModMenu{brls::getStr("page/addGame/menuTitle")}; // 添加模组菜单
    int m_focusedIndex = 0;             // 当前焦点索引

    /** @brief 设置页面标题 */
    void setHeader();

    /** @brief 获取游戏列表并设置待显示状态 */
    void prepareGames();

    /** @brief 配置添加模组菜单 */
    void setupMenu();

    /** @brief 初始化网格并注册回调 */
    void setupGridPage();

    /** @brief 首次 NACP 完成后启用搜索 */
    void enableSearch();

    /** @brief 按当前焦点提交下一张卡片 */
    void submitNextCard();

    /**
     * @brief 将纹理失效的卡片重新加入加载队列
     * @param gameIdx 游戏在列表中的索引
     */
    void queueCardReload(size_t gameIdx);

    /** @brief 获取虚拟游戏图标纹理 */
    int loadVirtualIcon();

    /**
     * @brief 获取或创建真实游戏图标纹理
     * @param key 纹理缓存 Key
     * @param image 解码后的游戏图标
     * @return 图标纹理 ID
     */
    int loadGameIcon(const std::string& key, const imageDecoder::DecodedImage& image);

    /**
     * @brief 在主线程应用卡片数据
     * @param appId 游戏唯一 ID
     * @param wasLoaded 游戏数据是否已经加载
     * @param name 游戏名称
     * @param version 游戏版本
     * @param image 解码后的游戏图标
     */
    void applyCard(uint64_t appId, bool wasLoaded, std::string name, std::string version, imageDecoder::DecodedImage image);

    /**
     * @brief 清除待显示状态并替换对应骨架
     * @param gameIdx 游戏在列表中的索引
     */
    void showCard(size_t gameIdx);

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
