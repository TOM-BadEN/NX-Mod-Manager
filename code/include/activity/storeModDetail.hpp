/**
 * StoreModDetail - 商店模组详情页面
 *
 * 左卡片（模组信息+描述） + 中卡片（截图） + 右卡片（留言板）
 * 进入页面后异步加载详情、截图和留言
 */

#pragma once
#include <borealis.hpp>
#include "view/recyclingGrid.hpp"
#include "view/myframe.hpp"
#include "view/circleButton.hpp"
#include "view/capsuleBadge.hpp"
#include "core/storeModDetailManager.hpp"
#include "utils/threadPool.hpp"
#include "core/gameManager.hpp"
#include "core/modManager.hpp"

class StoreModDetail : public brls::Activity {
public:
    using ReturnCallback = std::function<void(int likes, int dislikes, int downloads, bool downloaded)>;

    /**
     * @brief 构造商店模组详情页面
     * @param modId 模组 ID
     * @param gameTid 游戏 TID
     * @param gameName 游戏名称
     * @param gameManager 游戏数据管理
     * @param onReturn 返回列表页时的数据同步回调
     * @param localModManager 本地 ModManager（外部传入或页面补齐；本地游戏不存在时为空）
     * @param gameVersion 本地游戏版本号（"..."表示未安装/未知）
     * @param fromModList 是否从本地 ModList 进入
     */
    StoreModDetail(int modId, std::string gameTid, std::string gameName, GameManager& gameManager, ReturnCallback onReturn = nullptr, ModManager* localModManager = nullptr, std::string gameVersion = "...", bool fromModList = false);
    ~StoreModDetail();

    CONTENT_FROM_XML_RES("activity/storeModDetail.xml");

    // XML 绑定
    BRLS_BIND(MyFrame, m_frame, "storeModDetail/frame");

    // 左卡片（模组信息 + 描述）
    BRLS_BIND(brls::Box, m_leftCard, "storeModDetail/leftCard");
    BRLS_BIND(brls::Label, m_leftLoading, "storeModDetail/leftLoading");
    BRLS_BIND(brls::Box, m_leftContent, "storeModDetail/leftContent");
    BRLS_BIND(brls::Image, m_modIcon, "storeModDetail/modIcon");
    BRLS_BIND(brls::Label, m_modName, "storeModDetail/modName");
    BRLS_BIND(brls::Image, m_installedIcon, "storeModDetail/installedIcon");
    BRLS_BIND(brls::Box, m_installedUpdateBadge, "storeModDetail/installedUpdateBadge");
    BRLS_BIND(brls::Image, m_iconLike, "storeModDetail/iconLike");
    BRLS_BIND(brls::Image, m_iconDislike, "storeModDetail/iconDislike");
    BRLS_BIND(brls::Image, m_iconDownload, "storeModDetail/iconDownload");
    BRLS_BIND(brls::Image, m_iconTime, "storeModDetail/iconTime");
    BRLS_BIND(brls::Label, m_statLike, "storeModDetail/statLike");
    BRLS_BIND(brls::Label, m_statDislike, "storeModDetail/statDislike");
    BRLS_BIND(brls::Label, m_statDownload, "storeModDetail/statDownload");
    BRLS_BIND(brls::Label, m_statTime, "storeModDetail/statTime");
    BRLS_BIND(brls::Box, m_tagRow, "storeModDetail/tagRow");
    BRLS_BIND(CapsuleBadge, m_tagType, "storeModDetail/tagType");
    BRLS_BIND(CapsuleBadge, m_tagVersion, "storeModDetail/tagVersion");
    BRLS_BIND(CapsuleBadge, m_tagGameVersion, "storeModDetail/tagGameVersion");
    BRLS_BIND(CapsuleBadge, m_tagSize, "storeModDetail/tagSize");
    BRLS_BIND(CapsuleBadge, m_tagAuthor, "storeModDetail/tagAuthor");
    BRLS_BIND(brls::ScrollingFrame, m_scroll, "storeModDetail/scroll");
    BRLS_BIND(brls::Label, m_descBody, "storeModDetail/descBody");

    // 中卡片（截图 + 按钮）
    BRLS_BIND(brls::Box, m_middleCard, "storeModDetail/middleCard");
    BRLS_BIND(brls::Image, m_screenshot1, "storeModDetail/screenshot1");
    BRLS_BIND(brls::Image, m_screenshot2, "storeModDetail/screenshot2");
    BRLS_BIND(CircleButton, m_buttonLike, "storeModDetail/buttonLike");
    BRLS_BIND(CircleButton, m_buttonDislike, "storeModDetail/buttonDislike");
    BRLS_BIND(CircleButton, m_buttonDownload, "storeModDetail/buttonDownload");
    BRLS_BIND(CircleButton, m_buttonComment, "storeModDetail/buttonComment");

    // 右卡片
    BRLS_BIND(brls::Box, m_rightHintCard, "storeModDetail/rightHintCard");
    BRLS_BIND(brls::Label, m_rightHintLabel, "storeModDetail/rightHintLabel");
    BRLS_BIND(RecyclingGrid, m_commentGrid, "storeModDetail/commentGrid");

    /** @brief XML 加载完成后调用 */
    void onContentAvailable() override;

private:
    static constexpr const char* ANONYMOUS_NICKNAME = "__anonymous__"; // 与服务端约定的匿名留言标记，不是显示文案
    std::stop_source m_stopSource;                    // 页面级取消源（析构时取消所有任务）
    std::stop_source m_downloadStop;                   // 下载任务取消源（用户可单独取消下载）
    StoreModDetailManager m_manager;                 // 数据管理
    std::string m_gameName;                          // 页面标题（游戏名）
    GameManager& m_gameManager;                      // 游戏数据管理（Home 持有，引用传递）
    ReturnCallback m_onReturn;                       // 返回列表页时的数据同步回调
    ModManager* m_localModManager = nullptr;          // 本地 ModManager（外部传入或页面补齐；本地游戏不存在时为空）
    std::string m_gameVersion;                        // 本地游戏版本号（"..."表示未安装/未知）
    bool m_fromModList = false;                       // 是否从本地 ModList 进入

    bool m_detailLoaded = false;                     // 详情是否加载完成
    bool m_commentLoading = false;                   // 留言是否正在加载
    bool m_voteInFlight = false;                     // 投票请求是否进行中（防连点）

    /** @brief 左卡片初始化（图标主题、flexWrap、描述滚动、导航） */
    void setupLeftCard();

    /** @brief 截图初始化（焦点导航） */
    void setupScreenshots();

    /** @brief 按钮初始化（图标主题、点击、导航） */
    void setupButtons();

    /** @brief 右卡片初始化（注册 cell、分页回调） */
    void setupCommentGrid();

    /** @brief 异步加载模组详情 */
    void loadDetail();

    /** @brief 异步加载截图（两张并行） */
    void loadScreenshots();

    /** @brief 异步加载下一页留言 */
    void loadNextCommentPage();

    /** @brief 将留言列表中的固定匿名标记转换为当前语言显示 */
    static std::string displayCommentNickname(const std::string& nickname);

    /** @brief 用详情数据填充左卡片所有 Label */
    void updateDetail();

    /** @brief 根据本地 ModManager 填充下载和更新状态 */
    void applyLocalState();

    /** @brief 根据赞踩状态刷新图标和数字 */
    void updateVoteIcons();

    /** @brief 显示更新记录弹窗 */
    void showChangelogDialog();

    /** @brief 点赞按钮逻辑 */
    void onLikeAction();

    /** @brief 点踩按钮逻辑 */
    void onDislikeAction();

    /** @brief 下载按钮逻辑 */
    void onDownloadAction();

    /**
     * @brief 显示已下载状态下的下载提示
     * @param detail 当前模组详情
     * @return 是否已处理下载按钮逻辑
     */
    bool showDownloadedPrompt(const api::mod::ModDetail& detail);

    /** @brief 显示下载确认弹窗 */
    void showDownloadConfirmDialog();

    /**
     * @brief 开始下载或更新模组
     * @param updateMode 是否为更新模式
     */
    void startDownload(bool updateMode);

    /**
     * @brief 下载成功后在主线程保存模组并显示完成弹窗
     * @param gameTid 游戏 TID
     * @param gameNameEn 英文游戏名
     * @param gameName 当前显示游戏名
     * @param modDirName 模组目录名
     * @param tempPath 下载完成的临时 zip 路径
     * @param modName 当前显示模组名
     */
    void finishDownloadOnMainThread(const std::string& gameTid, const std::string& gameNameEn, const std::string& gameName, const std::string& modDirName, const std::string& tempPath, const std::string& modName);

    /**
     * @brief 更新成功后在主线程替换 zip 并刷新状态
     * @param tempPath 下载完成的临时 zip 路径
     * @param modName 当前显示模组名
     */
    void finishUpdateOnMainThread(const std::string& tempPath, const std::string& modName);

    /**
     * @brief 显示下载/更新完成弹窗
     * @param message 弹窗正文
     */
    void showCompleteDialog(const std::string& message);

    /** @brief 显示匿名留言提示弹窗 */
    void showAnonymousCommentPrompt();

    /** @brief 从匿名提示创建用户昵称 */
    void createNicknameFromCommentPrompt();

    /**
     * @brief 弹出留言输入框并提交留言
     * @param nickname 本次提交使用的昵称
     */
    void beginCommentInput(std::string nickname);

    /** @brief 留言按钮逻辑 */
    void onCommentAction();
};
