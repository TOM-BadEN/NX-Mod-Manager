/**
 * ZipReader - ZIP 读取封装实现
 * 基于 miniz 原生 API，仅负责读取，不涉及任何写盘操作
 */

#include "utils/zipReader.hpp"
#include "utils/textClean.hpp"

#include <cstring>
#include <set>

// ============================================================================
// 构造 / 析构
// ============================================================================

ZipReader::ZipReader(const std::string& zipPath) {
    memset(&m_archive, 0, sizeof(m_archive));

    if (!mz_zip_reader_init_file(&m_archive, zipPath.c_str(), 0)) return;
    m_open = true;

    int numEntries = static_cast<int>(mz_zip_reader_get_num_files(&m_archive));

    std::set<std::string> dirSet;

    for (int i = 0; i < numEntries; i++) {
        mz_zip_archive_file_stat stat;
        if (!mz_zip_reader_file_stat(&m_archive, i, &stat)) {
            m_open = false;
            return;
        }

        if (!textClean::isSafeRelativePath(stat.m_filename)) continue;

        if (mz_zip_reader_is_file_a_directory(&m_archive, i)) {
            std::string dir = stat.m_filename;
            if (!dir.empty() && dir.back() == '/') dir.pop_back();
            dirSet.insert(std::move(dir));
        } else {
            ZipEntry entry;
            entry.path = stat.m_filename;
            entry.crc32 = stat.m_crc32;
            entry.uncompressedSize = static_cast<int64_t>(stat.m_uncomp_size);
            entry.index = i;
            m_files.push_back(std::move(entry));

            const std::string& p = m_files.back().path;
            size_t pos = 0;
            while ((pos = p.find('/', pos + 1)) != std::string::npos) {
                dirSet.insert(p.substr(0, pos));
            }
        }
    }

    m_dirs.assign(dirSet.begin(), dirSet.end());
}

ZipReader::~ZipReader() {
    endRead();
    mz_zip_reader_end(&m_archive);
}

// ============================================================================
// 查询
// ============================================================================

bool ZipReader::isOpen() const {
    return m_open;
}

const std::vector<ZipEntry>& ZipReader::files() const {
    return m_files;
}

const std::vector<std::string>& ZipReader::dirs() const {
    return m_dirs;
}

// ============================================================================
// 一次性读取（小文件）
// ============================================================================

size_t ZipReader::readFile(const ZipEntry& entry, void* buf, size_t bufSize) {
    if (!m_open) return 0;

    size_t size = static_cast<size_t>(entry.uncompressedSize);
    if (size == 0 || size > bufSize) return 0;

    mz_bool ok = mz_zip_reader_extract_to_mem(&m_archive, entry.index, buf, size, 0);
    return ok ? size : 0;
}

// ============================================================================
// 流式读取（大文件）
// ============================================================================

bool ZipReader::beginRead(const ZipEntry& entry) {
    if (!m_open) return false;
    endRead();

    m_iter = mz_zip_reader_extract_iter_new(&m_archive, entry.index, 0);
    return m_iter != nullptr;
}

size_t ZipReader::read(void* buf, size_t bufSize) {
    if (!m_iter) return 0;
    return mz_zip_reader_extract_iter_read(m_iter, buf, bufSize);
}

void ZipReader::endRead() {
    if (m_iter) {
        mz_zip_reader_extract_iter_free(m_iter);
        m_iter = nullptr;
    }
}
