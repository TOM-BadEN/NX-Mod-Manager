/**
 * ModInstaller - 模块常量与共享工具函数
 */

#pragma once

#include "core/modInstaller/install.hpp"

#include <cstdlib>
#include <malloc.h>

namespace ModInstaller {

inline const std::string atmospherePath = "/atmosphere";
inline const std::string contentsPath = "/atmosphere/contents";

inline const std::vector<std::string> modKeywords = {
    "romfs", "exefs", "cheats", "exefs_patches", "romfs.bin"
};

inline constexpr const char* pchtxtExt = ".pchtxt";
inline constexpr size_t ioBufSize = 32 * 1024 * 1024;   // 32MB
inline constexpr size_t crcBufSize = 64 * 1024;          // 64KB

} // namespace ModInstaller

namespace ModInstaller::utils {

// ============================================================================
// 结构体
// ============================================================================

struct InstallBuf {
    void* io = nullptr;
    void* crc = nullptr;

    ~InstallBuf() { release(); }

    bool alloc() {
        io = memalign(0x1000, ioBufSize);
        crc = malloc(crcBufSize);
        return io && crc;
    }

    void release() {
        free(io);
        free(crc);
        io = nullptr;
        crc = nullptr;
    }
};

struct CreateDirsResult {
    bool success = true;
    std::string errorPath;
    std::string errorMsg;
};

// ============================================================================
// 工具函数
// ============================================================================

/**
 * @brief 获取路径最后一段（文件名）
 * @param path 文件路径
 */
const char* lastSegment(const std::string& path);

/**
 * @brief 判断字符串是否以指定后缀结尾
 * @param str 原字符串
 * @param suffix 后缀
 */
bool endsWith(const std::string& str, const std::string& suffix);

/**
 * @brief 判断路径是否包含 . 开头的路径段
 * @param path 文件或目录路径
 */
bool hasDotPathSegment(const std::string& path);

/**
 * @brief 查找路径中模组关键词的位置
 * @param path 文件路径
 */
size_t findKeywordPos(const std::string& path);

struct ModTidAndIpsDirs {
    std::vector<std::string> tidDirs;        // contents 下的 TID 目录名
    std::vector<std::string> ipsDirs;        // exefs_patches 下的子目录名
};

/**
 * @brief 扫描 mod 源目录，收集安装涉及的 TID 目录名和 exefs_patches 子目录名
 * @param mod 模组信息
 * @param game 游戏信息
 */
ModTidAndIpsDirs collectTidAndIpsDirs(const ModInfo& mod, const GameInfo& game);

/**
 * @brief 构建目标目录列表
 * @param dirs 源目录列表
 * @param tid 游戏 TID
 * @param skipDotEntries 是否跳过路径段以 . 开头的目录
 */
std::vector<std::string> buildTargetDirs(const std::vector<std::string>& dirs, const std::string& tid, bool skipDotEntries = false);

/**
 * @brief 构建单个文件的目标路径
 * @param path 源文件路径
 * @param tid 游戏 TID
 */
std::string buildTargetPath(const std::string& path, const std::string& tid);

/**
 * @brief 创建目录列表
 * @param dirs 需要创建的目录列表
 */
CreateDirsResult createDirs(const std::vector<std::string>& dirs);

/**
 * @brief 获取 ZIP 模组文件路径
 * @param modDir 模组目录
 */
std::string getZipModFilePath(const std::string& modDir);

/**
 * @brief 回滚已创建的文件和目录
 * @param createdFiles 已创建的文件列表
 * @param createdDirs 已创建的目录列表
 * @param progressCb 进度回调
 */
void rollback(const std::vector<std::string>& createdFiles, const std::vector<std::string>& createdDirs,
    std::function<void(const Progress&)>& progressCb);

struct PchtxtWriteResult {
    bool success = false;
    std::string ipsPath;
    std::string ipsDir;
    std::string errorMsg;
};

/**
 * @brief 将 pchtxt 转换为 IPS 并写入
 * @param data pchtxt 数据
 * @param len 数据长度
 * @param modDirName 模组目录名
 * @param gameDirName 游戏目录名（用于构建唯一 ips 目录）
 */
PchtxtWriteResult writePchtxt(const void* data, size_t len, const std::string& modDirName, const std::string& gameDirName);

/**
 * @brief 删除 pchtxt 对应的 IPS 补丁目录
 * @param modDirName 模组目录名
 * @param gameDirName 游戏目录名（用于构建唯一 ips 目录）
 */
void removePchtxt(const std::string& modDirName, const std::string& gameDirName);

/**
 * @brief 查找与目标文件冲突的模组名称
 * @param targetPath 目标文件路径
 * @param conflictCrc 源文件 CRC
 * @param allMods 所有模组列表
 * @param crcBuf CRC 计算缓冲区
 * @param crcBufLen 缓冲区长度
 */
std::string findConflictModName(const std::string& targetPath, uint32_t conflictCrc,
    const std::vector<ModInfo>& allMods, void* crcBuf, size_t crcBufLen, std::stop_token* token = nullptr);

} // namespace ModInstaller::utils
