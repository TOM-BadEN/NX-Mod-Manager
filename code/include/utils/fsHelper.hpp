/**
 * fsHelper - 文件系统操作封装
 * 基于 libnx 原生 API，纯工具函数，不含业务逻辑
 */

#pragma once

#include <string>
#include <vector>
#include <cstring>
#include <stop_token>
#include <functional>
#include <cstdint>
#include <switch.h>

// 路径包装：自动拷贝到栈上，避免堆指针传给 libnx IPC
struct FsPath {
    static constexpr int MAX_LEN = 0x301;  // 与 libnx FS_MAX_PATH 一致
    char s[MAX_LEN] = {};

    FsPath() = default;
    FsPath(const char* str) { std::strncpy(s, str, MAX_LEN - 1); }
    FsPath(const std::string& str) { std::strncpy(s, str.c_str(), MAX_LEN - 1); }

    operator const char*() const { return s; }
};

namespace fs {

    /**
     * @brief 目录是否存在
     * @param path 目录路径
     */
    bool dirExists(const FsPath& path);

    /**
     * @brief 文件是否存在
     * @param path 文件路径
     */
    bool fileExists(const FsPath& path);

    /**
     * @brief 确保目录存在，不存在则递归创建
     * @param path 目录路径
     */
    bool ensureDir(const FsPath& path);

    /**
     * @brief 单层创建目录，已存在返回 0，失败返回 libnx Result 错误码
     * @param path 目录路径
     */
    uint32_t createDir(const FsPath& path);
              
    /**
     * @brief 列出指定目录下的所有子目录名（不递归）
     * @param path 目录路径
     */
    std::vector<std::string> listSubDirs(const FsPath& path);

    /**
     * @brief 列出指定目录下的文件名（不递归）
     * @param path 目录路径
     * @param exts 扩展名过滤（空则返回所有文件）
     */
    std::vector<std::string> listSubFiles(const FsPath& path, const std::vector<std::string>& exts = {});

    /**
     * @brief 只数子目录数量
     * @param path 目录路径
     */
    int countDirs(const FsPath& path);

    /**
     * @brief 数指定目录下的条目数（子目录 + 文件）
     * @param path 目录路径
     * @param exts 扩展名过滤（空则数所有条目）
     */
    int countItems(const FsPath& path, const std::vector<std::string>& exts = {});

    struct DirEntry {
        std::string name;
        bool isFile;
        int64_t fileSize = 0;
    };

    /**
     * @brief 列出指定目录下的条目（子目录 + 文件），带类型信息
     * @param path 目录路径
     * @param exts 扩展名过滤（空则返回所有条目）
     */
    std::vector<DirEntry> listItems(const FsPath& path, const std::vector<std::string>& exts = {});

    /**
     * @brief 获取单个文件大小（字节），失败返回 -1
     * @param path 文件路径
     */
    int64_t getFileSize(const FsPath& path);

    /**
     * @brief 递归统计目录总大小（字节）
     * @param path 目录路径
     * @param token 取消令牌（可空）
     */
    int64_t calcDirSize(const FsPath& path, std::stop_token* token = nullptr);

    /**
     * @brief 递归删除目录及其所有内容
     * @param path 目录路径
     */
    bool removeDirAll(const FsPath& path);

    /**
     * @brief 递归删除目录内容，保留目录本身
     * @param path 目录路径
     */
    bool removeDirContents(const FsPath& path);

    struct DirCollection {
        std::string path;
        std::vector<DirEntry> entries;
    };

    struct RemoveResult {
        enum Status { Completed, Cancelled, FsError };

        Status status;
        int deletedCount;
        int totalCount;
        std::string errorPath;      // FsError 时：失败的文件/目录完整路径
        std::string errorCode;      // FsError 时：libnx Result 错误码（十六进制字符串）
        std::string elapsed;        // Completed 时：耗时（如 "12 秒"、"2 分 15 秒"）
    };

    /**
     * @brief 递归删除目录内容（保留目录），支持进度回调和取消
     * @param path 目录路径
     * @param token 取消令牌
     * @param onProgress 进度回调
     */
    RemoveResult removeDirContentsWithProgress(
        const FsPath& path,
        std::stop_token token,
        std::function<void(int deleted, int total, const char* fileName)> onProgress);

    /**
     * @brief 移动目录（同文件系统 rename）
     * @param src 源路径
     * @param dest 目标路径
     */
    bool moveDir(const FsPath& src, const FsPath& dest);

    /**
     * @brief 移动文件（同文件系统 rename）
     * @param src 源路径
     * @param dest 目标路径
     */
    bool moveFile(const FsPath& src, const FsPath& dest);

    /**
     * @brief 目标目录已存在时返回不冲突的路径（末尾加后缀）
     * @param path 目标路径
     */
    std::string ensureUniqueDirPath(const std::string& path);

    /**
     * @brief 删除单个文件，成功或不存在返回 true
     * @param path 文件路径
     */
    bool deleteFile(const FsPath& path);

    /**
     * @brief 删除空目录，目录非空返回 false
     * @param path 目录路径
     */
    bool deleteEmptyDir(const FsPath& path);

    /**
     * @brief 小文件一次性写入，成功返回 0，失败返回 libnx Result 错误码
     * @param path 文件路径
     * @param data 数据指针
     * @param size 数据大小
     */
    uint32_t writeFile(const FsPath& path, const void* data, size_t size);

    /**
     * @brief 小文件一次性读取，失败返回空数组
     * @param path 文件路径
     */
    std::vector<uint8_t> readFile(const FsPath& path);

    /** @brief 大文件流式读取句柄，RAII 管理生命周期 */
    class FileReader {
    public:
        FileReader() = default;
        ~FileReader();

        FileReader(const FileReader&) = delete;
        FileReader& operator=(const FileReader&) = delete;

        /**
         * @brief 打开文件
         * @param path 文件路径
         * @param fileSize 文件大小（>0 时跳过 fsFileGetSize）
         */
        uint32_t open(const FsPath& path, int64_t fileSize = -1);

        /**
         * @brief 读取一块数据，返回实际读取字节数，0 表示读完或出错
         * @param buf 缓冲区
         * @param bufSize 缓冲区大小
         */
        size_t read(void* buf, size_t bufSize);

        /** @brief 获取文件总大小（open 成功后有效） */
        int64_t size() const { return m_size; }

    private:
        FsFile m_handle{};
        int64_t m_offset = 0;
        int64_t m_size = 0;
        bool m_open = false;
    };

    /** @brief 大文件流式写入句柄，RAII 管理生命周期 */
    class FileWriter {
    public:
        FileWriter() = default;
        ~FileWriter();

        FileWriter(const FileWriter&) = delete;
        FileWriter& operator=(const FileWriter&) = delete;

        /**
         * @brief 创建并打开文件
         * @param path 文件路径
         * @param fileSize 文件大小
         */
        uint32_t open(const FsPath& path, int64_t fileSize);

        /**
         * @brief 写入一块数据，成功返回 0
         * @param data 数据指针
         * @param size 数据大小
         */
        uint32_t write(const void* data, size_t size);

    private:
        FsFile m_handle{};
        int64_t m_offset = 0;
        bool m_open = false;
    };

    /** @brief 目录流式读取句柄，RAII 管理生命周期 */
    class DirReader {
    public:
        DirReader() = default;
        ~DirReader();

        DirReader(const DirReader&) = delete;
        DirReader& operator=(const DirReader&) = delete;

        /**
         * @brief 打开目录
         * @param path 目录路径
         */
        uint32_t open(const FsPath& path);

        /**
         * @brief 读取一批条目，out 为空时表示读取完毕
         * @param out 输出条目列表
         * @param batchSize 每批数量
         */
        uint32_t read(std::vector<DirEntry>& out, int batchSize = 64);

        /** @brief 关闭目录句柄 */
        void close();

    private:
        FsDir m_handle{};
        bool m_open = false;
    };

} // namespace fs
