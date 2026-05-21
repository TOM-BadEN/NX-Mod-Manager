/**
 * ModList - Mod 列表页面
 * 左侧 RecyclingGrid（单列 Mod 列表） + 右侧 Mod 详情
 */

#pragma once

#include <borealis.hpp>
#include "view/myframe.hpp"
#include "view/recyclingGrid.hpp"
#include "core/modManager.hpp"
#include "core/gameManager.hpp"
#include "utils/threadPool.hpp"
#include "view/actionMenu.hpp"
#include "view/capsuleBadge.hpp"

class ModList : public brls::Activity {
public:
    ModList(size_t gameIndex, GameManager& gameManager);
    ~ModList() override;

    CONTENT_FROM_XML_RES("activity/modList.xml");

    BRLS_BIND(MyFrame, m_frame, "modList/frame");
    BRLS_BIND(RecyclingGrid, m_grid, "modList/grid");
    BRLS_BIND(brls::Box, m_detail, "modList/detail");
    BRLS_BIND(brls::ScrollingFrame, m_scroll, "modList/scroll");
    BRLS_BIND(brls::Box, m_tagRow, "modList/tagRow");

    // 详情面板绑定
    BRLS_BIND(brls::Image, m_gameIcon, "modList/gameIcon");
    BRLS_BIND(brls::Image, m_favIcon, "modList/favIcon");
    BRLS_BIND(brls::Label, m_gameNameLabel, "modList/gameName");
    BRLS_BIND(brls::Label, m_gameTid, "modList/gameTid");
    BRLS_BIND(CapsuleBadge, m_tagType, "modList/tagType");
    BRLS_BIND(CapsuleBadge, m_tagVersion, "modList/tagVersion");
    BRLS_BIND(CapsuleBadge, m_tagAuthor, "modList/tagAuthor");
    BRLS_BIND(CapsuleBadge, m_tagGameVer, "modList/tagGameVer");
    BRLS_BIND(CapsuleBadge, m_tagSize, "modList/tagSize");
    BRLS_BIND(CapsuleBadge, m_tagFormat, "modList/tagFormat");
    BRLS_BIND(brls::Label, m_descBody, "modList/descBody");

    /** @brief XML 加载完成后调用 */
    void onContentAvailable() override;

    /** @brief 页面恢复时刷新列表 */
    void onResume() override;

private:
    std::stop_source m_stopSource;            // 页面级取消源（退出页面时取消后台任务）
    std::stop_source m_installStop;            // 安装任务取消源（用户可取消安装）
    WaitableTask m_installTask;                // 安装/禁用/清理任务句柄（析构自动等待）
    GameManager& m_gameManager;              // 游戏数据管理（引用）
    size_t m_gameIndex;                      // 当前游戏索引
    ModManager m_modManager;                 // mod 数据管理
    size_t m_lastFocusIndex = 0;             // 切换到详情前记住的列表索引
    bool m_sizeLoading = false;              // 体积计算是否进行中
    int m_focusedIndex = 0;                  // 当前焦点索引（体积计算优先级）

    /**
     * @brief 切换模组安装/卸载状态
     * @param index 模组在列表中的索引
     */
    void toggleModInstall(int index);

    /** @brief 切换当前游戏模组禁用状态 */
    void toggleModDisable();

    /** @brief 初始化网格（注册 Cell、绑定数据源、设置回调） */
    void setupModGrid();

    /** @brief 初始化详情面板（游戏图标/名/TID） */
    void setupDetail();

    /**
     * @brief 根据焦点更新详情面板
     * @param index 当前焦点索引
     */
    void updateDetail(size_t index);

    /** @brief 切换排序方向 */
    void toggleSort();

    /**
     * @brief 刷新列表并聚焦到指定位置
     * @param index 目标焦点位置
     */
    void refreshAndFocus(int index);

    /** @brief 初始化菜单 */
    void setupMenu();

    /** @brief 初始化"管理模组"子菜单（含文件传输） */
    void setupManageMenu();

    /** @brief 从中转站添加模组 */
    void addModsFromTransit();

    /** @brief 移除当前模组到中转站 */
    void removeModFromList();

    /** @brief 移除最后一个模组并标记删除游戏 */
    void removeLastModFromList();

    /** @brief 强制清理：删除所有已安装 mod 文件并重置状态 */
    void forceClean();

    /** @brief 启动异步图标重试 */
    void startIconLoader();

    /** @brief 链式触发体积计算：主线程选最近 mod，提交一个计算任务，完成后自动触发下一个 */
    void submitNextSize();

    /**
     * @brief 应用体积结果到数据 + UI
     * @param modIdx 模组索引
     * @param sizeStr 格式化后的体积字符串
     */
    void applySizeResult(int modIdx, const std::string& sizeStr);

    /**
     * @brief 应用显示名称（写 JSON + 更新 Cell）
     * @param idx 模组索引
     * @param name 要设置的显示名称
     */
    void applyModDisplayName(int idx, const std::string& name);

    /** @brief 手动输入修改名称 */
    void manualSetModDisplayName();

    /** @brief 恢复名称 */
    void resetModDisplayName();

    /**
     * @brief 应用类型（写 JSON + 更新 Cell + 详情）
     * @param idx 模组索引
     * @param type 要设置的类型
     */
    void applyModType(int idx, const std::string& type);

    /** @brief 修改描述 */
    void editModDescription();

    /** @brief 修改 mod 版本 */
    void editModVersion();

    /** @brief 修改适配版本 */
    void editGameVersion();

    /** @brief 修改作者 */
    void editModAuthor();

    MenuPageConfig m_menu;                   // 主菜单
    MenuPageConfig m_addProjectMenu;         // 新增项目子菜单
    MenuPageConfig m_storeAddMenu;           // 商店添加子菜单
    MenuPageConfig m_editMenu;               // 修改项目子菜单
    MenuPageConfig m_typeMenu;               // 类型选择子菜单
    MenuPageConfig m_viewModMenu;             // 查看模组子菜单
    MenuPageConfig m_localAddMenu;           // 本地添加子菜单
    MenuPageConfig m_manageMenu;             // 管理项目子菜单

    /** @brief 初始化“新增项目”子菜单 */
    void setupAddProjectMenu();

    /** @brief 初始化“商店添加”子菜单 */
    void setupStoreAddMenu();

    /** @brief 初始化“本地添加”子菜单 */
    void setupLocalAddMenu();

    /** @brief 初始化“查看模组”子菜单 */
    void setupViewModMenu();

    /** @brief 初始化“修改项目”子菜单 */
    void setupEditMenu();

    /** @brief 设置用户昵称 */
    void setNickname();

    /** @brief 首次进入公告 */
    void runFirstLaunchDialog();
};