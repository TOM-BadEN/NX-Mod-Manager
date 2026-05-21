/**
 * ModManager - Mod 数据管理
 * 页面级数据层，由 ModList 持有成员对象，随页面创建/销毁。
 * 构造时加载 JSON + 扫描 mod 目录，提供排序、体积更新等操作。
 */

#pragma once

#include <vector>
#include <string>
#include "common/modInfo.hpp"
#include "common/gameInfo.hpp"
#include "core/modInstaller/install.hpp"
#include "core/modInstaller/utils.hpp"
#include "utils/jsonFile.hpp"
#include "utils/fsHelper.hpp"

class ModManager {
public:
    /**
     * @brief 构造时自动加载 JSON + 扫描 mod 目录
     * @param game 游戏信息
     */
    ModManager(const GameInfo& game);

    /**
     * @brief 快速检查是否有已安装 mod（不构造 ModManager）
     * @param tidPath TID 目录路径
     */
    static bool hasInstalledMod(const std::string& tidPath);

    /** @brief 中转站 mod 数量（子目录 + 匹配扩展名的文件） */
    static int transitModCount();

    /** @brief 中转站 mod 列表（子目录 + 匹配扩展名的文件），带类型信息 */
    static std::vector<fs::DirEntry> scanTransitMods();

    /**
     * @brief 将选中的中转站 mod 移动到游戏目录
     * @param dirPath 游戏目录路径
     * @param selectedMods 选中的中转站 mod 列表
     * @return 成功移动的数量
     */
    static int addModsFormTransit(const std::string& dirPath, const std::vector<fs::DirEntry>& selectedMods);

    /**
     * @brief 将选中的中转站 mod 移动到当前游戏目录，构造 ModInfo 并加入 m_mods
     * @param selectedMods 选中的中转站 mod 列表
     * @return 成功移动的数量
     */
    int addModsFormTransitForModList(const std::vector<fs::DirEntry>& selectedMods);

    /** @brief 获取 mod 列表引用 */
    std::vector<ModInfo>& mods();

    /** @brief 获取游戏信息引用 */
    const GameInfo& game() const;

    /** @brief 按当前方向排序 */
    void sort();

    /** @brief 切换排序方向并重新排序 */
    void toggleSortAsc();

    /** @brief 当前排序方向 */
    bool sortAsc() const;

    /**
     * @brief 设置显示名称
     * @param index mod 索引
     * @param name 显示名称
     */
    void setDisplayName(int index, const std::string& name);

    /**
     * @brief 删除自定义显示名，恢复为目录名
     * @param index mod 索引
     */
    void deleteCustomDisplayName(int index);

    /**
     * @brief 设置类型
     * @param index mod 索引
     * @param type 类型字符串
     */
    void setType(int index, const std::string& type);

    /**
     * @brief 设置描述
     * @param index mod 索引
     * @param desc 描述内容
     */
    void setDescription(int index, const std::string& desc);

    /**
     * @brief 设置 mod 版本
     * @param index mod 索引
     * @param version 版本字符串
     */
    void setModVersion(int index, const std::string& version);

    /**
     * @brief 设置适配的游戏版本
     * @param index mod 索引
     * @param version 游戏版本字符串
     */
    void setGameVersion(int index, const std::string& version);

    /**
     * @brief 设置作者
     * @param index mod 索引
     * @param author 作者名称
     */
    void setAuthor(int index, const std::string& author);

    /**
     * @brief 更新 mod 体积到数据 + JSON 缓存
     * @param index mod 索引
     * @param sizeStr 格式化后的体积字符串
     */
    void setSize(int index, const std::string& sizeStr);

    /** @brief 保存 JSON 缓存 */
    void saveJson();

    /**
     * @brief 商店下载后添加 mod 到内存列表
     * @param info mod 信息
     */
    void addModFromStore(ModInfo info);

    /**
     * @brief 设置待聚焦 modID（商店下载后设置，ModList::onResume 消费）
     * @param modID 模组 ID
     */
    void setPendingFocus(int modID);

    /** @brief 消费待聚焦 modID */
    int consumePendingFocus();

    /**
     * @brief 按 modID 查找索引（-1 = 未找到）
     * @param modID 模组 ID
     */
    int findByModID(int modID) const;

    /**
     * @brief 移除 mod（移到中转站、清 JSON、从列表移除）
     * @param idx mod 索引
     */
    void removeModFromModList(int idx);

    /**
     * @brief 按目录名查找索引（-1 = 未找到）
     * @param dirName 目录名
     */
    int findByDirName(const std::string& dirName) const;

    /**
     * @brief 安装 mod，成功后更新 isInstalled + JSON
     * @param index mod 索引
     * @param progressCb 进度回调
     * @param token 取消令牌
     */
    ModInstaller::InstallResult installMod(int index,
        std::function<void(const ModInstaller::Progress&)> progressCb = nullptr,
        std::stop_token token = {});

    /**
     * @brief 卸载 mod，成功后更新 isInstalled + JSON
     * @param index mod 索引
     * @param progressCb 进度回调
     */
    ModInstaller::UninstallResult uninstallMod(int index,
        std::function<void(const ModInstaller::Progress&)> progressCb = nullptr);

    /**
     * @brief 禁用当前游戏所有模组
     *   - rename /atmosphere/contents/{tid} → {tid}-disable（存在才 rename）
     *   - 遍历所有 mod，检测 /atmosphere/exefs_patches/{dirName}_{gameDirName}/ 是否存在，
     *     存在则将目录内所有 .ips 文件 rename 为 .ips-disable
     * @return true 成功，false contents 目录 rename 失败
     */
    bool disableMods();

    /**
     * @brief 取消禁用，恢复所有模组
     *   - rename /atmosphere/contents/{tid}-disable → {tid}（存在才 rename）
     *   - 遍历所有 mod，检测 /atmosphere/exefs_patches/{dirName}_{gameDirName}/ 是否存在，
     *     存在则将目录内所有 .ips-disable 文件 rename 回 .ips
     * @return true 成功，false contents 目录 rename 失败
     */
    bool enableMods();

    /**
     * @brief 强制清理：删除该游戏所有已安装 mod 文件、ips 补丁、引用计数，重置安装状态
     * @param token 取消令牌
     * @param onProgress 进度回调
     */
    fs::RemoveResult forceClean(std::stop_token token, std::function<void(int deleted, int total, const char* fileName)> onProgress);

private:
    /** @brief 遍历所有已安装 mod，收集涉及的 TID 和 exefs_patches 目录（含游戏 TID 保底） */
    ModInstaller::utils::ModTidAndIpsDirs collectAllTidAndIpsDirs();

    /** @brief 排序实现（三级：已安装 > 未安装 → 类型分组 → 拼音） */
    void sort(bool ascending);

    const GameInfo& m_game;                 // 游戏信息（引用 GameManager 中的元素）
    std::vector<ModInfo> m_mods;            // mod 列表
    JsonFile m_modJson;                     // mod 元数据 JSON 缓存
    int m_pendingFocusModID = -1;           // 待聚焦 modID
    bool m_sortAsc = true;                  // 排序方向
};
