/**
 * Based on ftpsrv by ITotalJustice
 * https://github.com/ITotalJustice/ftpsrv
 *
 * Minimal SD card VFS implementation for Nintendo Switch.
 * Defines the required FtpVfsFile/Dir/DirEntry structs
 * and is included via FTP_VFS_HEADER by ftpsrv_vfs.h.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <switch.h>

struct FtpVfsFile {
    FsFile fd;
    FsFileSystem* fs;
    s64 off;
    s64 chunk_size;
    bool is_write;
    bool is_valid;
};

struct FtpVfsDir {
    FsDir dir;
    FsFileSystem* fs;
    bool is_valid;
    bool is_virtual_root;  /* opendir("/") 时为 true，readdir 返回快捷目录列表 */
    int virtual_index;     /* 虚拟根 readdir 迭代索引 */
};

struct FtpVfsDirEntry {
    FsDirectoryEntry buf;
};

/* 快捷目录挂载 API（ftp.cpp 调用） */
#define FTP_VFS_MAX_MOUNTS 8
#define FTP_VFS_NAME_MAX 64

struct FtpVfsMountEntry {
    char displayName[FTP_VFS_NAME_MAX]; /* FTP 客户端看到的目录名 */
    char basePath[FS_MAX_PATH];         /* SD 卡实际路径 */
};

void ftp_vfs_set_mounts(const struct FtpVfsMountEntry* mounts, int count);
void ftp_vfs_clear_mounts(void);

#ifdef __cplusplus
}
#endif
