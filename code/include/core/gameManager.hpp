/**
 * GameManager - 游戏数据管理
 * 作为游戏数据的总调度层，内部持有游戏列表和 JSON 缓存
 * Home 页面通过此类完成所有游戏相关的数据操作
 */

#pragma once

#include "api/game.hpp"
#include "common/gameInfo.hpp"
#include "common/settings.hpp"
#include "utils/fsHelper.hpp"
#include "utils/gameNacp.hpp"
#include "utils/jsonFile.hpp"

#include <stop_token>
#include <string>
#include <vector>

/** @brief 游戏列表排序模式 */
enum class SortMode {
    Name,       // 按游戏名称排序
    ModCount,   // 按模组数量排序
    RecentPlay, // 按最近游玩顺序排序
};

class GameManager {
public:
    /** @brief 构造时自动加载 JSON + 扫描游戏目录 */
    GameManager();

    /**
     * @brief 获取游戏列表引用
     * @return 游戏列表引用
     */
    std::vector<GameInfo>& games();

    /**
     * @brief 获取游戏目录名（从 dirPath 提取倒数第二段）
     * @param idx 游戏索引
     * @return 游戏目录名
     */
    std::string getDirName(int idx);

    /**
     * @brief 获取重复 appId 的游戏数量
     * @return 重复 appId 的游戏数量
     */
    int duplicateCount() const;

    /** @brief 用当前排序参数重新排序 */
    void sort();

    /**
     * @brief 设置排序模式并重置默认方向（持久化）
     * @param mode 排序模式
     */
    void setSortMode(SortMode mode);

    /** @brief 翻转升降序（自动重排） */
    void toggleSortAsc();

    /**
     * @brief 查询当前排序模式
     * @return 当前排序模式
     */
    SortMode sortMode() const;

    /**
     * @brief 查询当前升降序
     * @return 升序返回 true，降序返回 false
     */
    bool sortAsc() const;

    /**
     * @brief 按 appId 查找游戏索引（找不到返回 -1）
     * @param appId 游戏 appId
     * @return 游戏索引，找不到时返回 -1
     */
    int findByAppId(uint64_t appId);

    /**
     * @brief 按项目路径查找游戏索引（找不到返回 -1）
     * @param dirPath 游戏项目路径
     * @return 游戏索引，找不到时返回 -1
     */
    int findByDirPath(const std::string& dirPath);

    /**
     * @brief 按 appId 查找所有游戏索引
     * @param appId 游戏 appId
     * @return 所有匹配的游戏索引
     */
    std::vector<int> findAllByAppId(uint64_t appId);

    /**
     * @brief 按 appId 查找已安装游戏索引（找不到返回 -1）
     * @param appId 游戏 appId
     * @return 已安装游戏索引，找不到时返回 -1
     */
    int findInstalledByAppId(uint64_t appId);

    /**
     * @brief 判断游戏是否已在本机安装
     * @param appId 游戏 appId
     * @return 游戏已安装时返回 true，否则返回 false
     */
    bool isGameInstalled(uint64_t appId);

    /**
     * @brief 获取 NACP 元数据（名称、版本、图标）
     * @param idx 游戏索引
     * @return 游戏 NACP 元数据
     */
    GameMetadata fetchMetadata(int idx);

    /**
     * @brief 按 appId 获取 NACP 元数据（名称、版本、图标）
     * @param appId 游戏 appId
     * @return 游戏 NACP 元数据
     */
    GameMetadata fetchMetadataByAppId(uint64_t appId);

    /**
     * @brief 在线获取游戏显示名称
     * @param idx 游戏索引
     * @param token 取消令牌
     * @return 游戏名称查询结果
     */
    api::game::NameResult fetchDisplayName(int idx, std::stop_token token);

    /**
     * @brief 设置收藏状态
     * @param idx 游戏索引
     * @param favorite 是否收藏
     */
    void setFavorite(int idx, bool favorite);

    /**
     * @brief 设置模组禁用状态
     * @param idx 游戏索引
     * @param disabled 是否禁用
     */
    void setModsDisabled(int idx, bool disabled);

    /**
     * @brief 设置游戏已安装模组标记
     * @param idx 游戏索引
     * @param value 是否有已安装模组
     */
    void setHasInstalledMod(int idx, bool value);

    /**
     * @brief 设置游戏模组数量，并同步已安装游戏缓存
     * @param idx 游戏索引
     * @param modCount 模组数量
     */
    void setModCount(int idx, int modCount);

    /**
     * @brief 设置版本号
     * @param idx 游戏索引
     * @param version 版本字符串
     * @param save 是否立即持久化
     */
    void setVersion(int idx, const std::string& version, bool save = true);

    /**
     * @brief 设置官方名称，仅在无自定义 displayName 时同步到显示名
     * @param idx 游戏索引
     * @param name 官方名称
     * @param save 是否立即持久化
     */
    void setGameName(int idx, const std::string& name, bool save = true);

    /**
     * @brief 设置显示名称
     * @param idx 游戏索引
     * @param name 显示名称
     */
    void setDisplayName(int idx, const std::string& name);

    /**
     * @brief 获取回滚显示名称（gameName → 目录名）
     * @param idx 游戏索引
     * @return 回滚后的显示名称
     */
    std::string getRestoredDisplayName(int idx);

    /**
     * @brief 删除自定义显示名，恢复为指定名称
     * @param idx 游戏索引
     * @param restoredName 恢复后的名称
     */
    void deleteCustomDisplayName(int idx, const std::string& restoredName);

    /**
     * @brief 添加游戏（检查是否已存在，不存在则创建目录 + 写 JSON）
     * @param installedIdx 已安装游戏索引
     * @param modCount 模组数量
     * @param iconId Home 永久持有的图标纹理 ID
     * @return 游戏目录路径
     */
    std::string addGame(size_t installedIdx, int modCount, int iconId);

    /**
     * @brief 商店下载时创建新游戏项目
     * @param gameTid 游戏 TID
     * @param gameNameEn 游戏英文名
     * @param gameName 游戏名称
     * @return 游戏目录路径
     */
    std::string addNewGameFromStore(const std::string& gameTid, const std::string& gameNameEn, const std::string& gameName);

    /**
     * @brief 商店下载时更新已有游戏项目
     * @param dirPath 已有游戏项目路径
     * @return 游戏目录路径
     */
    std::string addExistingGameFromStore(const std::string& dirPath);

    /**
     * @brief 虚拟游戏（Switch 未安装）时，按 TID 查找或创建游戏目录
     * @param tid 游戏 TID 十六进制字符串
     * @param modCount 本次添加的模组数量
     * @return 游戏目录路径
     */
    std::string addGameByTid(const std::string& tid, int modCount);

    /**
     * @brief 移除游戏（MOD 移到中转站，删除游戏目录，清理 JSON）
     * @param idx 游戏索引
     * @return 相同 appId 的游戏已全部移除时返回 true
     */
    bool removeGame(int idx);

    /**
     * @brief 删除游戏（删除全部 MOD、游戏目录和 JSON）
     * @param idx 游戏索引
     * @return 相同 appId 的游戏已全部删除时返回 true
     */
    bool deleteGame(int idx);

    /**
     * @brief 清理空游戏项目及相关记录
     * @param idx 游戏索引
     * @return 相同 appId 的游戏已全部移除时返回 true
     */
    bool cleanupGame(int idx);

    /** @brief 删除游戏项目内容的结果 */
    struct DeleteGameContentsResult {
        fs::RemoveResult removeResult; // 当前模组的文件删除结果
        int remainingModCount;         // 删除停止后剩余的模组数量
    };

    /**
     * @brief 逐个删除游戏项目中的模组及其 JSON 记录
     * @param idx 游戏索引
     * @param onProgress 进度回调
     * @return 文件删除结果和剩余模组数量
     */
    DeleteGameContentsResult deleteGameContents(int idx, std::function<void(int, int, const char*)> onProgress);

    /**
     * @brief 清空中转站
     * @param token 取消令牌
     * @param onProgress 进度回调
     * @return 删除任务原始结果
     */
    fs::RemoveResult clearTransit(std::stop_token token, std::function<void(int, int, const char*)> onProgress);

    /**
     * @brief 重置所有游戏的安装状态（不删除 MOD 文件）
     * @param onProgress 进度回调(当前序号, 总数, 游戏显示名)
     */
    void resetState(std::function<void(int, int, const std::string&)> onProgress);

    /** @brief 统一保存 JSON 缓存（批量操作后调用） */
    void saveJsonCache();

    /**
     * @brief 获取已安装游戏 TID 列表（懒加载）
     * @return 已安装游戏 TID 列表引用
     */
    const std::vector<uint64_t>& getInstalledTids();

    /**
     * @brief 获取已安装游戏列表（首次调用加载，后续返回缓存）
     * @return 已安装游戏列表引用
     */
    std::vector<InstalledGameInfo>& getInstalledGames();

    /**
     * @brief 排序已安装游戏列表
     * @param ascending 是否升序
     */
    void sortInstalledGames(bool ascending);

    /**
     * @brief 设置待聚焦游戏项目路径（页面更新后设置，Home onResume 时消费）
     * @param dirPath 游戏项目路径
     */
    void setPendingFocusPath(const std::string& dirPath);

    /**
     * @brief 消费待聚焦游戏项目路径
     * @return 待聚焦游戏项目路径，没有待处理游戏时返回空字符串
     */
    std::string consumePendingFocusPath();

    /**
     * @brief 设置待清理游戏（ModList 处理最后一个 mod 后设置，Home onResume 时消费）
     * @param dirPath 游戏项目路径
     */
    void setPendingCleanup(const std::string& dirPath);

    /**
     * @brief 消费待清理游戏项目路径
     * @return 待清理游戏项目路径，没有待处理游戏时返回空字符串
     */
    std::string consumePendingCleanup();

private:
    std::vector<GameInfo> m_games;                  // 游戏列表
    JsonFile m_jsonCache;                            // JSON 缓存
    int m_duplicateCount = 0;                        // 重复 appId 数量

    std::vector<InstalledGameInfo> m_installedGames; // 已安装游戏列表缓存
    bool m_installedGamesLoaded = false;             // 已安装游戏是否已加载
    std::string m_pendingFocusDirPath;               // 待聚焦游戏项目路径
    std::string m_pendingCleanupDirPath;             // 待清理游戏项目路径
    std::vector<uint64_t> m_installedTids;           // 已安装游戏 TID 缓存（懒加载）
    bool m_installedTidsLoaded = false;              // 已安装游戏 TID 是否已查询（结果允许为空）
    SortMode m_sortMode = SortMode::Name;            // 当前排序模式
    bool m_sortAsc = true;                           // 当前升降序

    /** @brief 从 Settings 读取排序配置 */
    void loadSortSettings();

    /**
     * @brief 内部排序实现
     * @param mode 排序模式
     * @param ascending 是否升序
     */
    void sort(SortMode mode, bool ascending);

    /**
     * @brief 按标题拼音排序
     * @param ascending 是否升序
     */
    void sortByName(bool ascending);

    /**
     * @brief 按模组数量排序（同数量拼音兜底）
     * @param ascending 是否升序
     */
    void sortByModCount(bool ascending);

    /**
     * @brief 按最近游玩排序（不在列表的拼音兜底）
     * @param ascending 是否升序
     */
    void sortByRecentPlay(bool ascending);
};
