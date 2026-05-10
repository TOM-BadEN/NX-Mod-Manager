/**
 * GameManager - 游戏数据管理
 * 作为游戏数据的总调度层，内部持有游戏列表和 JSON 缓存
 * Home 页面通过此类完成所有游戏相关的数据操作
 */

#pragma once

#include <string>
#include <vector>
#include <stop_token>
#include "common/gameInfo.hpp"
#include "utils/jsonFile.hpp"
#include "core/gameNACP.hpp"
#include "api/game.hpp"
#include "utils/fsHelper.hpp"

class GameManager {

public:
    /** @brief 构造时自动加载 JSON + 扫描游戏目录 */
    GameManager();

    /** @brief 获取游戏列表引用 */
    std::vector<GameInfo>& games();

    /**
     * @brief 获取游戏的 appId hex 字符串
     * @param idx 游戏索引
     */
    std::string getAppIdKey(int idx);

    /**
     * @brief 获取游戏目录名（从 dirPath 提取倒数第二段）
     * @param idx 游戏索引
     */
    std::string getDirName(int idx);

    /** @brief 重复 appId 的游戏数量 */
    int duplicateCount() const;

    /**
     * @brief 排序游戏列表
     * @param ascending 是否升序
     */
    void sort(bool ascending);

    /**
     * @brief 按 appId 查找游戏索引（找不到返回 -1）
     * @param appId 游戏 appId
     */
    int findByAppId(uint64_t appId);

    /**
     * @brief 按 appId 查找已安装游戏索引（找不到返回 -1）
     * @param appId 游戏 appId
     */
    int findInstalledByAppId(uint64_t appId);

    /** @brief 打开 NACP 服务 */
    void openNacp();

    /** @brief 关闭 NACP 服务 */
    void closeNacp();

    /**
     * @brief 获取 NACP 元数据（名称、版本、图标）
     * @param idx 游戏索引
     */
    GameMetadata fetchMetadata(int idx);

    /**
     * @brief 在线获取游戏显示名称
     * @param idx 游戏索引
     * @param token 取消令牌
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
     * @return 游戏目录路径
     */
    std::string addGame(size_t installedIdx, int modCount);

    /**
     * @brief 商店下载时查找或创建游戏目录
     * @param gameTid 游戏 TID
     * @param gameNameEn 游戏英文名
     * @param gameName 游戏名称
     * @return 游戏目录路径
     */
    std::string addGameFromStore(const std::string& gameTid, const std::string& gameNameEn, const std::string& gameName);

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
     */
    void removeGame(int idx);

    /**
     * @brief 清空中转站
     * @param token 取消令牌
     * @param onProgress 进度回调
     */
    fs::RemoveResult clearTransit(
        std::stop_token token,
        std::function<void(int, int, const char*)> onProgress);

    /**
     * @brief 重置所有游戏的安装状态（不删除 MOD 文件）
     * @param onProgress 进度回调(当前序号, 总数, 游戏显示名)
     */
    void resetState(std::function<void(int, int, const std::string&)> onProgress);

    /** @brief 统一保存 JSON 缓存（批量操作后调用） */
    void saveJsonCache();

    /** @brief 获取已安装游戏列表（首次调用加载，后续返回缓存） */
    std::vector<InstalledGameInfo>& getInstalledGames();

    /**
     * @brief 获取已安装游戏的 NACP 元数据
     * @param idx 已安装游戏索引
     */
    GameMetadata fetchInstalledMetadata(size_t idx);

    /**
     * @brief 排序已安装游戏列表
     * @param ascending 是否升序
     */
    void sortInstalledGames(bool ascending);

    /**
     * @brief 设置待聚焦游戏（商店下载/添加游戏后设置，Home onResume 时消费）
     * @param appId 游戏 appId
     */
    void setPendingFocus(uint64_t appId);

    /** @brief 消费待聚焦游戏 appId */
    uint64_t consumePendingFocus();
    
private:
    std::vector<GameInfo> m_games;
    JsonFile m_jsonCache;
    GameNACP* m_nacp = nullptr;
    int m_duplicateCount = 0;

    std::vector<InstalledGameInfo> m_installedGames;
    bool m_installedGamesLoaded = false;
    uint64_t m_pendingFocusAppId = 0;

};
