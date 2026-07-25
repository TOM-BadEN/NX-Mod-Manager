/**
 * Based on ftpsrv by ITotalJustice
 * https://github.com/ITotalJustice/ftpsrv
 *
 * Minimal SD card VFS implementation for Nintendo Switch.
 * All ftp_vfs_* functions required by ftpsrv.c are implemented here,
 * targeting only the SD card via fsFsXxx libnx APIs.
 */
#include "ftpsrv_vfs.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <switch.h>

#define NX_WRITE_CHUNK_SIZE (1024ULL * 1024ULL * 1ULL)

static FsFileSystem* g_sdmc_fs = NULL;

/* 快捷目录挂载表 */
static struct FtpVfsMountEntry g_mounts[FTP_VFS_MAX_MOUNTS];
static int g_mount_count = 0;

void ftp_vfs_set_mounts(const struct FtpVfsMountEntry* mounts, int count) {
    if (count > FTP_VFS_MAX_MOUNTS) count = FTP_VFS_MAX_MOUNTS;
    for (int i = 0; i < count; i++) {
        g_mounts[i] = mounts[i];
    }
    g_mount_count = count;
}

void ftp_vfs_clear_mounts(void) {
    g_mount_count = 0;
}

static int vfs_sd_set_errno(Result rc) {
    if (R_VALUE(rc) == 0x202)  { errno = ENOENT;  return -1; }
    if (R_VALUE(rc) == 0x402)  { errno = EEXIST;  return -1; }
    if (R_VALUE(rc) == 0x4E02) { errno = ENOSPC;  return -1; }
    errno = EIO;
    return -1;
}

static FsFileSystem* get_sdmc_fs(void) {
    if (!g_sdmc_fs) {
        g_sdmc_fs = fsdevGetDeviceFileSystem("sdmc");
    }
    return g_sdmc_fs;
}

/**
 * 将 FTP 路径转换为 SD 卡实际路径
 *
 * 无快捷目录时：直接去掉 "sdmc:" 前缀，确保前导 /
 * 有快捷目录时：匹配前缀名，拼上对应的 basePath
 * 例如：/mods/xxx → /mods2/xxx（displayName="mods", basePath="/mods2"）
 */
static const char* translate_path(const char* path, char nxpath[FS_MAX_PATH]) {
    /* skip "sdmc:" prefix if present */
    if (path[0] == 's' && path[1] == 'd' && path[2] == 'm' && path[3] == 'c' && path[4] == ':') {
        path += 5;
    }

    /* 有快捷目录时，匹配前缀名并路由到实际路径 */
    if (g_mount_count > 0) {
        const char* p = (path[0] == '/') ? path + 1 : path;
        for (int i = 0; i < g_mount_count; i++) {
            size_t len = strlen(g_mounts[i].displayName);
            if (!strncmp(p, g_mounts[i].displayName, len) && (p[len] == '/' || p[len] == '\0')) {
                const char* rest = p + len; /* "/xxx" 或 "" */
                snprintf(nxpath, FS_MAX_PATH, "%s%s", g_mounts[i].basePath, rest);
                return nxpath;
            }
        }
        /* 未匹配任何快捷目录 */
        errno = ENOENT;
        return NULL;
    }

    /* 无快捷目录：原始逻辑 */
    if (path[0] != '/') {
        nxpath[0] = '/';
        strncpy(nxpath + 1, path, FS_MAX_PATH - 2);
        nxpath[FS_MAX_PATH - 1] = '\0';
    } else {
        strncpy(nxpath, path, FS_MAX_PATH - 1);
        nxpath[FS_MAX_PATH - 1] = '\0';
    }
    return nxpath;
}

int ftp_vfs_open(struct FtpVfsFile* f, const char* path, enum FtpVfsOpenMode mode) {
    FsFileSystem* fs = get_sdmc_fs();
    if (!fs) { errno = EIO; return -1; }

    char nxpath[FS_MAX_PATH];
    if (!translate_path(path, nxpath)) return -1;

    u32 open_mode;
    if (mode == FtpVfsOpenMode_READ) {
        open_mode = FsOpenMode_Read;
        f->is_write = false;
    } else {
        fsFsCreateFile(fs, nxpath, 0, 0);
        open_mode = FsOpenMode_Write;
        f->is_write = true;
    }

    Result rc;
    if (R_FAILED(rc = fsFsOpenFile(fs, nxpath, open_mode, &f->fd))) {
        return vfs_sd_set_errno(rc);
    }

    f->fs = fs;
    f->off = 0;
    f->chunk_size = 0;

    if (mode == FtpVfsOpenMode_WRITE) {
        if (R_FAILED(rc = fsFileSetSize(&f->fd, 0))) {
            fsFileClose(&f->fd);
            return vfs_sd_set_errno(rc);
        }
    } else if (mode == FtpVfsOpenMode_APPEND) {
        if (R_FAILED(rc = fsFileGetSize(&f->fd, &f->off))) {
            fsFileClose(&f->fd);
            return vfs_sd_set_errno(rc);
        }
        f->chunk_size = f->off;
    }

    f->is_valid = true;
    return 0;
}

int ftp_vfs_read(struct FtpVfsFile* f, void* buf, size_t size) {
    u64 bytes_read;
    Result rc = fsFileRead(&f->fd, f->off, buf, size, FsReadOption_None, &bytes_read);
    if (R_FAILED(rc)) {
        return vfs_sd_set_errno(rc);
    }
    f->off += bytes_read;
    return (int)bytes_read;
}

int ftp_vfs_write(struct FtpVfsFile* f, const void* buf, size_t size) {
    Result rc;

    if (f->chunk_size < f->off + (s64)size) {
        f->chunk_size += NX_WRITE_CHUNK_SIZE;
        if (R_FAILED(rc = fsFileSetSize(&f->fd, f->chunk_size))) {
            return vfs_sd_set_errno(rc);
        }
    }

    if (R_FAILED(rc = fsFileWrite(&f->fd, f->off, buf, size, FsWriteOption_None))) {
        return vfs_sd_set_errno(rc);
    }

    f->off += size;
    return (int)size;
}

int ftp_vfs_seek(struct FtpVfsFile* f, const void* buf, size_t size, size_t off) {
    (void)buf;
    (void)size;
    f->off = off;
    return 0;
}

int ftp_vfs_close(struct FtpVfsFile* f) {
    if (!f->is_valid) {
        return -1;
    }
    if (f->chunk_size) {
        fsFileSetSize(&f->fd, f->off); /* shrink to actual size */
    }
    fsFileClose(&f->fd);
    f->is_valid = false;
    return 0;
}

int ftp_vfs_isfile_open(struct FtpVfsFile* f) {
    return f->is_valid;
}

int ftp_vfs_opendir(struct FtpVfsDir* f, const char* path) {
    f->is_virtual_root = false;
    f->virtual_index = 0;

    /* 虚拟根目录：有快捷目录且路径为 "/" 时，返回快捷列表而非 SD 卡根目录 */
    if (g_mount_count > 0 && (!path || !strcmp(path, "/"))) {
        f->is_virtual_root = true;
        f->is_valid = true;
        f->fs = NULL;
        return 0;
    }

    FsFileSystem* fs = get_sdmc_fs();
    if (!fs) { errno = EIO; return -1; }

    char nxpath[FS_MAX_PATH];
    if (!translate_path(path, nxpath)) return -1;

    Result rc;
    if (R_FAILED(rc = fsFsOpenDirectory(fs, nxpath, FsDirOpenMode_ReadDirs | FsDirOpenMode_ReadFiles, &f->dir))) {
        return vfs_sd_set_errno(rc);
    }

    f->fs = fs;
    f->is_valid = true;
    return 0;
}

const char* ftp_vfs_readdir(struct FtpVfsDir* f, struct FtpVfsDirEntry* entry) {
    /* 虚拟根：依次返回快捷目录名 */
    if (f->is_virtual_root) {
        if (f->virtual_index >= g_mount_count) return NULL;
        memset(&entry->buf, 0, sizeof(entry->buf));
        strncpy(entry->buf.name, g_mounts[f->virtual_index].displayName, sizeof(entry->buf.name) - 1);
        entry->buf.type = FsDirEntryType_Dir;
        f->virtual_index++;
        return entry->buf.name;
    }

    s64 total_entries;
    Result rc = fsDirRead(&f->dir, &total_entries, 1, &entry->buf);
    if (R_FAILED(rc) || total_entries <= 0) {
        return NULL;
    }
    return entry->buf.name;
}

int ftp_vfs_dirlstat(struct FtpVfsDir* f, const struct FtpVfsDirEntry* entry, const char* path, struct stat* st) {
    memset(st, 0, sizeof(*st));
    st->st_nlink = 1;

    /* 虚拟根的快捷目录始终是目录类型 */
    if (f->is_virtual_root) {
        st->st_mode = S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO;
        return 0;
    }

    if (entry->buf.type == FsDirEntryType_File) {
        st->st_size = entry->buf.file_size;
        st->st_mode = S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;

        /* Try to get timestamps */
        if (f->fs) {
            char nxpath[FS_MAX_PATH];
            if (translate_path(path, nxpath)) {
                FsTimeStampRaw timestamp;
                if (R_SUCCEEDED(fsFsGetFileTimeStampRaw(f->fs, nxpath, &timestamp)) && timestamp.is_valid) {
                    st->st_ctime = timestamp.created;
                    st->st_mtime = timestamp.modified;
                    st->st_atime = timestamp.accessed;
                }
            }
        }
    } else {
        st->st_mode = S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO;
    }

    return 0;
}

int ftp_vfs_closedir(struct FtpVfsDir* f) {
    if (!f->is_valid) {
        return -1;
    }
    if (!f->is_virtual_root) {
        fsDirClose(&f->dir);
    }
    f->is_valid = false;
    f->is_virtual_root = false;
    return 0;
}

int ftp_vfs_isdir_open(struct FtpVfsDir* f) {
    return f->is_valid;
}

int ftp_vfs_stat(const char* path, struct stat* st) {
    /* 虚拟根目录或快捷名称本身：返回目录类型 */
    if (g_mount_count > 0) {
        if (!path || !strcmp(path, "/")) {
            memset(st, 0, sizeof(*st));
            st->st_nlink = 1;
            st->st_mode = S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO;
            return 0;
        }
        /* 检查是否为快捷目录名称本身（如 "/mods"） */
        const char* p = (path[0] == '/') ? path + 1 : path;
        for (int i = 0; i < g_mount_count; i++) {
            if (!strcmp(p, g_mounts[i].displayName)) {
                memset(st, 0, sizeof(*st));
                st->st_nlink = 1;
                st->st_mode = S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO;
                return 0;
            }
        }
    }

    FsFileSystem* fs = get_sdmc_fs();
    if (!fs) { errno = EIO; return -1; }

    char nxpath[FS_MAX_PATH];
    if (!translate_path(path, nxpath)) return -1;

    Result rc;
    FsDirEntryType type;
    if (R_FAILED(rc = fsFsGetEntryType(fs, nxpath, &type))) {
        return vfs_sd_set_errno(rc);
    }

    memset(st, 0, sizeof(*st));
    st->st_nlink = 1;

    if (type == FsDirEntryType_File) {
        FsFile file;
        if (R_FAILED(rc = fsFsOpenFile(fs, nxpath, FsOpenMode_Read, &file))) {
            return vfs_sd_set_errno(rc);
        }
        s64 size;
        if (R_FAILED(rc = fsFileGetSize(&file, &size))) {
            fsFileClose(&file);
            return vfs_sd_set_errno(rc);
        }
        st->st_size = size;
        st->st_mode = S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;

        FsTimeStampRaw timestamp;
        if (R_SUCCEEDED(fsFsGetFileTimeStampRaw(fs, nxpath, &timestamp)) && timestamp.is_valid) {
            st->st_ctime = timestamp.created;
            st->st_mtime = timestamp.modified;
            st->st_atime = timestamp.accessed;
        }
        fsFileClose(&file);
    } else {
        st->st_mode = S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO;
    }

    return 0;
}

int ftp_vfs_lstat(const char* path, struct stat* st) {
    return ftp_vfs_stat(path, st); /* Switch has no symlinks */
}

int ftp_vfs_mkdir(const char* path) {
    FsFileSystem* fs = get_sdmc_fs();
    if (!fs) { errno = EIO; return -1; }

    char nxpath[FS_MAX_PATH];
    if (!translate_path(path, nxpath)) return -1;

    Result rc;
    if (R_FAILED(rc = fsFsCreateDirectory(fs, nxpath))) {
        return vfs_sd_set_errno(rc);
    }
    return 0;
}

int ftp_vfs_unlink(const char* path) {
    FsFileSystem* fs = get_sdmc_fs();
    if (!fs) { errno = EIO; return -1; }

    char nxpath[FS_MAX_PATH];
    if (!translate_path(path, nxpath)) return -1;

    Result rc;
    if (R_FAILED(rc = fsFsDeleteFile(fs, nxpath))) {
        return vfs_sd_set_errno(rc);
    }
    return 0;
}

int ftp_vfs_rmdir(const char* path) {
    FsFileSystem* fs = get_sdmc_fs();
    if (!fs) { errno = EIO; return -1; }

    char nxpath[FS_MAX_PATH];
    if (!translate_path(path, nxpath)) return -1;

    Result rc;
    if (R_FAILED(rc = fsFsDeleteDirectoryRecursively(fs, nxpath))) {
        return vfs_sd_set_errno(rc);
    }
    return 0;
}

int ftp_vfs_rename(const char* src, const char* dst) {
    FsFileSystem* fs = get_sdmc_fs();
    if (!fs) { errno = EIO; return -1; }

    char nxpath_src[FS_MAX_PATH];
    char nxpath_dst[FS_MAX_PATH];
    if (!translate_path(src, nxpath_src)) return -1;
    if (!translate_path(dst, nxpath_dst)) return -1;

    Result rc;
    FsDirEntryType type;
    if (R_FAILED(rc = fsFsGetEntryType(fs, nxpath_src, &type))) {
        return vfs_sd_set_errno(rc);
    }

    if (type == FsDirEntryType_File) {
        if (R_FAILED(rc = fsFsRenameFile(fs, nxpath_src, nxpath_dst))) {
            return vfs_sd_set_errno(rc);
        }
    } else {
        if (R_FAILED(rc = fsFsRenameDirectory(fs, nxpath_src, nxpath_dst))) {
            return vfs_sd_set_errno(rc);
        }
    }
    return 0;
}

int ftp_vfs_readlink(const char* path, char* buf, size_t buflen) {
    (void)path;
    (void)buf;
    (void)buflen;
    errno = ENOSYS; /* Switch has no symlinks */
    return -1;
}

const char* ftp_vfs_getpwuid(const struct stat* st) {
    (void)st;
    return "switch";
}

const char* ftp_vfs_getgrgid(const struct stat* st) {
    (void)st;
    return "switch";
}
