/**
 * ModInstaller - 模块常量与共享工具函数
 */

#pragma once

#include "core/modInstaller/install.hpp"

#include <cstdlib>
#include <malloc.h>

namespace ModInstaller {

inline const std::string atmospherePath = "/atmosphere";          // Atmosphere 根目录
inline const std::string contentsPath = "/atmosphere/contents";   // 游戏内容覆盖目录

/** @brief 用于定位 MOD 内容根目录的路径关键词 */
inline const std::vector<std::string> modKeywords = {
    "romfs", "romfslite", "exefs", "cheats", "exefs_patches", "romfs.bin"
};

inline constexpr const char* pchtxtExt = ".pchtxt";         // pchtxt 文件扩展名
inline constexpr size_t ioBufSize = 32 * 1024 * 1024;   // 32MB
inline constexpr size_t crcBufSize = 64 * 1024;          // 64KB

} // namespace ModInstaller

namespace ModInstaller::utils {

// ============================================================================
// 结构体
// ============================================================================

/** @brief 安装过程使用的 I/O 与 CRC 缓冲区 */
struct InstallBuf {
    void* io = nullptr;    // 文件读写缓冲区
    void* crc = nullptr;   // CRC 计算缓冲区

    /** @brief 释放已分配的缓冲区 */
    ~InstallBuf() { release(); }

    /**
     * @brief 分配安装缓冲区
     * @return 是否全部分配成功
     */
    bool alloc() {
        io = memalign(0x1000, ioBufSize);
        crc = malloc(crcBufSize);
        return io && crc;
    }

    /** @brief 释放安装缓冲区 */
    void release() {
        free(io);
        free(crc);
        io = nullptr;
        crc = nullptr;
    }
};

/** @brief 批量创建目录结果 */
struct CreateDirsResult {
    bool success = true;       // 是否全部创建成功
    std::string errorPath;     // 失败时的目录路径
    std::string errorMsg;      // 失败原因
};

// ============================================================================
// 工具函数
// ============================================================================

/**
 * @brief 获取路径最后一段（文件名）
 * @param path 文件路径
 * @return 指向文件名起始位置的指针
 */
const char* lastSegment(const std::string& path);

/**
 * @brief 判断字符串是否以指定后缀结尾
 * @param str 原字符串
 * @param suffix 后缀
 * @return 是否以指定后缀结尾
 */
bool endsWith(const std::string& str, const std::string& suffix);

/**
 * @brief 判断路径是否包含 . 开头的路径段
 * @param path 文件或目录路径
 * @return 是否包含隐藏路径段
 */
bool hasDotPathSegment(const std::string& path);

/**
 * @brief 查找路径中模组关键词的位置
 * @param path 文件路径
 * @return 关键词起始位置，未找到时返回 std::string::npos
 */
size_t findKeywordPos(const std::string& path);

/** @brief MOD 安装涉及的 TID 与 IPS 目录集合 */
struct ModTidAndIpsDirs {
    std::vector<std::string> tidDirs;        // contents 下的 TID 目录名
    std::vector<std::string> ipsDirs;        // exefs_patches 下的子目录名
};

/**
 * @brief 扫描 mod 源目录，收集安装涉及的 TID 目录名和 exefs_patches 子目录名
 * @param mod 模组信息
 * @param game 游戏信息
 * @return 扫描得到的 TID 与 IPS 目录集合
 */
ModTidAndIpsDirs collectTidAndIpsDirs(const ModInfo& mod, const GameInfo& game);

/**
 * @brief 构建目标目录列表
 * @param dirs 源目录列表
 * @param tid 游戏 TID
 * @param skipDotEntries 是否跳过路径段以 . 开头的目录
 * @return 目标目录列表
 */
std::vector<std::string> buildTargetDirs(const std::vector<std::string>& dirs, const std::string& tid, bool skipDotEntries = false);

/**
 * @brief 构建单个文件的目标路径
 * @param path 源文件路径
 * @param tid 游戏 TID
 * @return 目标路径，无法识别时返回空字符串
 */
std::string buildTargetPath(const std::string& path, const std::string& tid);

/**
 * @brief 创建目录列表
 * @param dirs 需要创建的目录列表
 * @return 批量创建目录结果
 */
CreateDirsResult createDirs(const std::vector<std::string>& dirs);

/**
 * @brief 获取 ZIP 模组文件路径
 * @param modDir 模组目录
 * @return ZIP 文件路径，未找到时返回空字符串
 */
std::string getZipModFilePath(const std::string& modDir);

/**
 * @brief 回滚已创建的文件和目录
 * @param createdFiles 已创建的文件列表
 * @param createdDirs 已创建的目录列表
 * @param progressCb 进度回调
 */
void rollback(const std::vector<std::string>& createdFiles, const std::vector<std::string>& createdDirs, std::function<void(const Progress&)>& progressCb);

/** @brief pchtxt 转换和写入结果 */
struct PchtxtWriteResult {
    bool success = false;      // 是否转换并写入成功
    std::string ipsPath;       // 生成的 IPS 文件路径
    std::string ipsDir;        // 生成的 IPS 目录路径
    std::string errorMsg;      // 失败原因
};

/**
 * @brief 将 pchtxt 转换为 IPS 并写入
 * @param data pchtxt 数据
 * @param len 数据长度
 * @param modDirName 模组目录名
 * @param gameDirName 游戏目录名（用于构建唯一 ips 目录）
 * @return pchtxt 转换和写入结果
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
 * @param token 取消令牌，可为空
 * @return 冲突模组名称，未找到时返回未知模组文本
 */
std::string findConflictModName(const std::string& targetPath, uint32_t conflictCrc, const std::vector<ModInfo>& allMods, void* crcBuf, size_t crcBufLen, std::stop_token* token = nullptr);

} // namespace ModInstaller::utils
