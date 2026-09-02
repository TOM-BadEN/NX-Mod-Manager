/**
 * ModInstaller - 模组安装目标策略
 */

#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace ModInstaller::targets {

/**
 * @brief 受管理目标的生命周期类型
 *
 * contents 使用整棵 Title ID 目录；IPS 补丁使用一个补丁集目录。区分两者
 * 后，ModManager 可以为不同目标选择正确的禁用、恢复和清理操作。
 */
enum class ManagedTargetKind {
    ContentsDirectory,  // /atmosphere/contents/<TID>，通过目录改名禁用
    IpsDirectory,       // /atmosphere/{exefs,nro}_patches/<补丁集>，通过文件后缀禁用
};

/**
 * @brief 可由 ModManager 禁用、恢复或清理的目标
 *
 * path 是 Atmosphere 下的目标路径，而不是模组源目录中的相对路径。kind
 * 决定生命周期操作是重命名目录，还是重命名其中的 IPS 文件。
 */
struct ManagedTarget {
    ManagedTargetKind kind;
    std::string path;
};

/** @brief 单个模组安装目标的路径规则 */
class TargetStrategy {
public:
    virtual ~TargetStrategy() = default;

    /**
     * @brief 在源路径中查找当前策略的根目录
     * @param path 模组中的相对目录或文件路径
     * @param pos 输出根目录在 path 中的起始位置
     * @return 是否找到该策略负责的根目录
     */
    virtual bool findKeywordPos(const std::string& path, size_t& pos) const = 0;

    /**
     * @brief 判断当前策略是否接受该源文件
     *
     * 目录识别和文件接收是两个独立步骤。nro_patches 目录可以用于创建目标
     * 目录，但只有符合固定层级且扩展名为 .ips 的文件才能安装。
     */
    virtual bool acceptsFile(const std::string& path, size_t pos) const = 0;

    /**
     * @brief 生成单个文件的 Atmosphere 目标路径
     * @param path 模组中的相对文件路径
     * @param pos 根目录在 path 中的起始位置
     * @param gameTid 当前游戏 Title ID
     */
    virtual std::string buildTargetPath(const std::string& path, size_t pos, const std::string& gameTid) const = 0;

    /**
     * @brief 为源目录追加需要创建的目标目录
     *
     * 实现需要自行去重，因为目录扫描通常同时包含根目录、补丁集目录和更深层目录。
     */
    virtual void appendTargetDirs(const std::string& path, size_t pos, const std::string& gameTid, std::vector<std::string>& dirs) const = 0;

    /**
     * @brief 是否允许仅凭源目录登记生命周期目标
     *
     * contents 的目录结构本身就能确定目标；NRO IPS 必须看到合法 IPS 文件后
     * 才登记补丁集，避免空目录或说明文件被当成已安装补丁。
     */
    virtual bool collectsManagedTargetsFromDirs() const = 0;

    /**
     * @brief 为源路径追加需要由管理器维护的生命周期目标
     * @param path 模组中的相对目录或文件路径
     * @param pos 根目录在 path 中的起始位置
     * @param gameTid 当前游戏 Title ID
     * @param targets 输出的生命周期目标集合
     */
    virtual void appendManagedTargets(const std::string& path, size_t pos, const std::string& gameTid, std::vector<ManagedTarget>& targets) const = 0;
};

/** @brief 安装目标策略注册表 */
class TargetStrategyRegistry {
public:
    /**
     * @brief 查找能匹配源路径的策略，未匹配时返回 nullptr
     *
     * 当路径同时包含多个关键词时，注册表选择源路径中最靠前的根目录，解决
     * nro_patches/romfs/<BuildID>.ips 这类补丁集名称与 contents 关键词重名的问题。
     */
    static const TargetStrategy* find(const std::string& path, size_t& pos);
};

/** @brief 查找任一已注册目标策略的根目录位置 */
size_t findKeywordPos(const std::string& path);

/** @brief 构建单个文件的目标路径，无法识别或不被接受时返回空字符串 */
std::string buildTargetPath(const std::string& path, const std::string& gameTid);

/** @brief 构建目录安装或 ZIP 解压前所需创建的目标目录 */
std::vector<std::string> buildTargetDirs(const std::vector<std::string>& sourceDirs, const std::string& gameTid);

/** @brief 收集模组涉及的生命周期目标 */
std::vector<ManagedTarget> collectManagedTargets(const std::vector<std::string>& sourceDirs, const std::vector<std::string>& sourceFiles, const std::string& gameTid);

} // namespace ModInstaller::targets
