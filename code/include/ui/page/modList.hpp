/**
 * ModList - Mod 列表页面
 * 左侧 RecyclingGrid（单列 Mod 列表） + 右侧 Mod 详情
 */

#pragma once

#include <borealis.hpp>
#include "ui/core/page.hpp"
#include "ui/core/shellState.hpp"
#include "ui/view/recyclingGrid.hpp"
#include "core/modManager.hpp"
#include "core/gameManager.hpp"
#include "utils/threadPool.hpp"
#include "ui/view/contextMenu.hpp"
#include "ui/view/capsuleBadge.hpp"
#include "ui/view/scrollHint.hpp"

class ModList : public Page, public ShellState {
public:
    ModList(size_t gameIndex, GameManager& gameManager);
    ~ModList() override;

    BRLS_BIND(RecyclingGrid, m_grid, "modList/grid");
    BRLS_BIND(brls::Box, m_detail, "modList/detail");
    BRLS_BIND(brls::ScrollingFrame, m_scroll, "modList/scroll");
    BRLS_BIND(brls::Box, m_scrollContent, "modList/scrollContent");
    BRLS_BIND(ScrollHint, m_scrollHint, "modList/scrollHint");
    BRLS_BIND(brls::Box, m_tagRow, "modList/tagRow");

    // 详情面板绑定
    BRLS_BIND(brls::Image, m_gameIcon, "modList/gameIcon");
    BRLS_BIND(brls::Image, m_favIcon, "modList/favIcon");
    BRLS_BIND(brls::Label, m_gameNameLabel, "modList/gameName");
    BRLS_BIND(brls::Label, m_gameTid, "modList/gameTid");
    BRLS_BIND(brls::Label, m_tagType, "modList/tagTypeText");
    BRLS_BIND(brls::Label, m_tagVersion, "modList/tagVersionText");
    BRLS_BIND(brls::Label, m_tagAuthor, "modList/tagAuthorText");
    BRLS_BIND(brls::Label, m_tagGameVer, "modList/tagGameVerText");
    BRLS_BIND(brls::Label, m_tagSize, "modList/tagSizeText");
    BRLS_BIND(brls::Label, m_tagFormat, "modList/tagFormatText");
    BRLS_BIND(brls::Label, m_descBody, "modList/descBody");

    /** @brief XML 加载完成后调用 */
    void onContentAvailable() override;

    /** @brief 布局更新后刷新底部滚动提示 */
    void onLayout() override;

    /** @brief 页面恢复时刷新列表 */
    void onResume() override;

private:
    std::stop_source m_stopSource;            // 页面级取消源（退出页面时取消后台任务）
    std::stop_source m_installStop;            // 安装任务取消源（用户可取消安装）
    size_t m_iconRetryDelayId = 0;             // 1 秒重试定时器句柄,0 表示未调度
    WaitableTask m_installTask;                // 安装/清理任务句柄（析构自动等待）
    GameManager& m_gameManager;                // 游戏数据管理（引用）
    size_t m_gameIndex;                        // 当前游戏索引
    ModManager m_modManager;                   // mod 数据管理
    size_t m_lastFocusIndex = 0;               // 切换到详情前记住的列表索引
    bool m_metadataLoading = false;            // 体积和 CRC32 顺序流程是否进行中
    int m_focusedIndex = 0;                    // 当前焦点索引（元数据任务优先级）
    bool m_layoutReady = false;                // 页面内容是否已经完成初始化

    /** @brief 设置页面标题 */
    void setHeader();

    /**
     * @brief 切换模组安装/卸载状态
     * @param index 模组在列表中的索引
     */
    void toggleModInstall(int index);

    /** @brief 显示模组安装或卸载确认弹窗 */
    void showModInstallDialog(int index);

    /** @brief 启动模组安装或卸载任务 */
    void startModInstallTask(int index);

    /** @brief 检查当前游戏的 MOD 安装条件 */
    bool checkBeforeModInstall(int index);

    /** @brief 检查怪猎 MOD 安装条件，失败时弹出提示 */
    bool checkMHRiseRules();

    /** @brief 检查饥荒 MOD 安装条件 */
    bool checkDontStarveRules(int index);

    /** @brief 切换当前游戏模组禁用状态 */
    void toggleModDisable();

    /** @brief 初始化网格（注册 Cell、绑定数据源、设置回调） */
    void setupModGrid();

    /** @brief 按当前焦点提交下一张卡片 */
    void submitNextCard();

    /**
     * @brief 结束骨架并显示完整卡片
     * @param dirName 模组稳定目录名
     */
    void showCard(const std::string& dirName);

    /** @brief 初始化详情面板（游戏图标/名/TID） */
    void setupDetail();

    /** @brief 根据焦点和描述高度更新底部滚动提示 */
    void updateScrollHintVisibility();

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

    /**
     * @brief 完成新增模组后的页面数据更新，并返回新增模组索引
     * @param sortMods 是否在定位新增模组前排序
     * @return 新增模组索引
     */
    int finalizeModAddition(bool sortMods);

    /** @brief 初始化菜单 */
    void setupMenu();

    /** @brief 从中转站添加模组 */
    void addModsFromTransit();

    /** @brief 移除当前模组到中转站 */
    void removeModFromList();

    /** @brief 永久删除当前模组 */
    void deleteModFromList();

    /**
     * @brief 后台删除指定模组
     * @param idx 模组索引
     */
    void startDeleteMod(int idx);

    /** @brief 移除最后一个模组并标记移除游戏 */
    void removeLastModFromList();

    /** @brief 删除最后一个模组并标记删除游戏 */
    void deleteLastModFromList();

    /** @brief 强制清理：删除所有已安装 mod 文件并重置状态 */
    void forceClean();

    /** @brief 设置游戏图标，首次查询失败时延迟重试一次 */
    void setGameIcon();

    /**
     * @brief 启动体积和 CRC32 顺序流程
     * @param checkUpdatesWhenDone 本轮任务结束后是否检查更新
     */
    void startMetadataLoader(bool checkUpdatesWhenDone);

    /**
     * @brief 按当前焦点提交下一个体积和 CRC32 任务
     * @param checkUpdatesWhenDone 本轮任务结束后是否检查更新
     */
    void submitNextMetadata(bool checkUpdatesWhenDone);

    /**
     * @brief 在主线程应用体积和 CRC32 结果
     * @param dirName 模组稳定目录名
     * @param sizeStr 格式化后的体积字符串，空字符串表示未取得
     * @param crc32 CRC32 字符串，空字符串表示未取得
     */
    void applyMetadataResult(const std::string& dirName, const std::string& sizeStr, const std::string& crc32);

    /** @brief CRC32 全部就绪后异步检查模组更新，并单独刷新命中的卡片 */
    void submitUpdateCheck();

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

    ContextMenuPage m_menu{brls::getStr("page/modList/mainMenuTitle")};          // 主菜单
    ContextMenuPage m_editMenu{brls::getStr("page/modList/editMenuTitle")};      // 编辑项目子菜单
    ContextMenuPage m_typeMenu{brls::getStr("page/modList/typeMenuTitle")};      // 类型选择子菜单
    ContextMenuPage m_localAddMenu{brls::getStr("page/modList/localAdd")};       // 本地添加子菜单
    ContextMenuPage m_assistFeaturesMenu{brls::getStr("page/modList/assistFeatures")}; // 辅助功能子菜单

    /** @brief 初始化“本地添加”子菜单 */
    void setupLocalAddMenu();

    /** @brief 初始化“辅助功能”子菜单 */
    void setupAssistFeaturesMenu();

    /** @brief 打开当前选中模组的商店详情，无商店记录时显示提示 */
    void openStoreModDetail();

    /** @brief 初始化“编辑项目”子菜单 */
    void setupEditMenu();

    /** @brief 创建通用首次进入公告内容 */
    brls::Box* createFirstLaunchBox();

    /** @brief 创建怪猎首次进入公告内容 */
    brls::Box* createMHRiseFirstLaunchBox();

    /** @brief 创建饥荒首次进入公告内容 */
    brls::Box* createDontStarveFirstLaunchBox();

    /** @brief 创建本地模组提取提示内容 */
    brls::Box* createUnmanagedModBox();

    /** @brief 显示怪猎首次进入公告 */
    void showMHRiseFirstLaunchDialog();

    /** @brief 显示饥荒首次进入公告 */
    void showDontStarveFirstLaunchDialog();

    /** @brief 根据游戏类型显示特殊游戏首次进入公告 */
    void showSpecialGameFirstLaunchDialog();

    /** @brief 显示本地模组提取提示 */
    void showUnmanagedModDialog();

    /** @brief 提取本地模组 */
    void startUnmanagedModExtraction();

    /** @brief 首次进入公告 */
    void runFirstLaunchDialog();
};
