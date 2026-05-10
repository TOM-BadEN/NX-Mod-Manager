/**
 * ZipReader - ZIP 读取封装
 * 基于 miniz，RAII 管理 ZIP 句柄，构造时打开，析构时关闭
 * 仅负责读取，不涉及任何写盘操作
 */
#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <miniz.h>

// ZIP 文件条目信息（从中央目录读取，不含目录条目）
struct ZipEntry {
    std::string path;           // ZIP 内相对路径
    uint32_t crc32;             // CRC32 校验值
    int64_t uncompressedSize;   // 解压后大小（字节）
    int index;                  // miniz 文件索引
};

class ZipReader {
public:
    /**
     * @brief 构造时打开 ZIP 文件并缓存所有条目信息
     * @param zipPath ZIP 文件路径
     */
    ZipReader(const std::string& zipPath);

    /** @brief 析构时自动关闭 ZIP 文件 */
    ~ZipReader();

    ZipReader(const ZipReader&) = delete;
    ZipReader& operator=(const ZipReader&) = delete;

    /** @brief ZIP 文件是否成功打开 */
    bool isOpen() const;

    /** @brief 获取所有文件条目（构造时已缓存，不含目录） */
    const std::vector<ZipEntry>& files() const;

    /** @brief 获取所有目录路径（构造时已去重排序，不含尾斜杠） */
    const std::vector<std::string>& dirs() const;

    /**
     * @brief 将指定条目一次性解压到外部缓冲区
     * @param entry ZIP 文件条目
     * @param buf 缓冲区
     * @param bufSize 缓冲区大小
     * @return 实际解压字节数，0 表示失败
     */
    size_t readFile(const ZipEntry& entry, void* buf, size_t bufSize);

    /**
     * @brief 开始流式读取指定条目
     * @param entry ZIP 文件条目
     */
    bool beginRead(const ZipEntry& entry);

    /**
     * @brief 读取一块数据，0 表示读完或出错
     * @param buf 缓冲区
     * @param bufSize 缓冲区大小
     */
    size_t read(void* buf, size_t bufSize);

    /** @brief 结束当前流式读取，释放内部迭代器 */
    void endRead();

private:
    mz_zip_archive m_archive{};
    bool m_open = false;
    std::vector<ZipEntry> m_files;
    std::vector<std::string> m_dirs;
    mz_zip_reader_extract_iter_state* m_iter = nullptr;  // 当前流式读取迭代器
};
