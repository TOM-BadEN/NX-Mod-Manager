/**
 * Copyright 2024 TotalJustice.
 * SPDX-License-Identifier: MIT
 * https://github.com/ITotalJustice/ftpsrv
 */
#include "ftpsrv.h"
#include "ftpsrv_vfs.h"
#include "ftpsrv_socket.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>

#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <errno.h>

// helper which returns the size of array
#define FTP_ARR_SZ(x) (sizeof(x) / sizeof(x[0]))

// number of max concurrent sessions
#ifndef FTP_MAX_SESSIONS
    #define FTP_MAX_SESSIONS 128
#endif

// size of the buffer used for file transfers
#ifndef FTP_FILE_BUFFER_SIZE
    #define FTP_FILE_BUFFER_SIZE (1024 * 64) /* 64 KiB */
#endif

// size of the max length of pathname
#ifndef FTP_PATHNAME_SIZE
    #define FTP_PATHNAME_SIZE 4096
#endif

// the below shouldn't be messed with, it will *not* improve
// performance and will greatly increase memory usage at higher values.
#ifndef FTP_LISTBUF_SIZE
    #define FTP_LISTBUF_SIZE 1024
#endif

#ifndef FTP_CMDBUF_SIZE
    #define FTP_CMDBUF_SIZE 1024
#endif

#ifndef FTP_SENDBUF_SIZE
    #define FTP_SENDBUF_SIZE 1024
#endif

#define TELNET_EOL "\r\n"

enum FTP_TYPE {
    FTP_TYPE_ASCII,  // unsupported
    FTP_TYPE_EBCDIC, // unsupported
    FTP_TYPE_IMAGE,
    FTP_TYPE_LOCAL,  // unsupported
};

enum FTP_MODE {
    FTP_MODE_STREAM,
    FTP_MODE_BLOCK,      // unsupported
    FTP_MODE_COMPRESSED, // unsupported
};

enum FTP_STRUCTURE {
    FTP_STRUCTURE_FILE,
    FTP_STRUCTURE_RECORD, // unsupported
    FTP_STRUCTURE_PAGE,   // unsupported
};

enum FTP_DATA_CONNECTION {
    FTP_DATA_CONNECTION_NONE,    // default, starts in control
    FTP_DATA_CONNECTION_ACTIVE,  // enabled using PORT
    FTP_DATA_CONNECTION_PASSIVE, // enabled using PASV
};

enum FTP_TRANSFER_MODE {
    FTP_TRANSFER_MODE_NONE, // no transfer
    FTP_TRANSFER_MODE_RETR, // transfer using RETR
    FTP_TRANSFER_MODE_STOR, // transfer using STOR
    FTP_TRANSFER_MODE_LIST, // transfer using LIST
    FTP_TRANSFER_MODE_NLST, // transfer using NLST
    FTP_TRANSFER_MODE_MLSD, // transfer using NLST
};

enum FTP_AUTH_MODE {
    FTP_AUTH_MODE_NONE,      // not authenticated
    FTP_AUTH_MODE_NEED_PASS, // username ok, waiting for password
    FTP_AUTH_MODE_VALID,     // authenticated
};

enum FTP_SESSION_STATE {
    FTP_SESSION_STATE_NONE,     // not active.
    FTP_SESSION_STATE_POLLIN,   // waiting for commands.
    FTP_SESSION_STATE_POLLOUT,  // sending message to client.
    // FTP_SESSION_STATE_BLOCKING, // executing a command which is not async, but may block.
};

enum FTP_FILE_TRANSFER_STATE {
    FTP_FILE_TRANSFER_STATE_CONTINUE, // ready to transfer data.
    FTP_FILE_TRANSFER_STATE_FINISHED, // all data transfered, close connection.
    FTP_FILE_TRANSFER_STATE_BLOCKING, // transfer if blocking, exit loop.
    FTP_FILE_TRANSFER_STATE_ERROR,    // error during transfer, close connection.
};

struct Pathname {
    char s[FTP_PATHNAME_SIZE];
};

struct FtpTransfer {
    enum FTP_TRANSFER_MODE mode;
    bool connection_pending;

    size_t offset;
    size_t size; // only set during RETR, LIST and NLIST.
    size_t index; // only used for NLIST and LIST devices.

    struct FtpVfsFile file_vfs;
    struct FtpVfsDir dir_vfs;

    char list_buf[FTP_LISTBUF_SIZE];
};

struct FtpSession {
    enum FTP_SESSION_STATE state;
    enum FTP_AUTH_MODE auth_mode;
    enum FTP_TYPE type;
    enum FTP_MODE mode;
    enum FTP_STRUCTURE structure;
    enum FTP_DATA_CONNECTION data_connection;

    struct FtpTransfer transfer;

    struct FtpSocket control_sock; // socket for commands
    struct FtpSocket data_sock;    // socket for data (PORT/PASV)
    struct FtpSocket pasv_sock;    // socket for PASV listen fd

    struct sockaddr_in control_sockaddr;
    struct sockaddr_in data_sockaddr;
    struct sockaddr_in pasv_sockaddr;

    size_t server_marker; // file offset when using REST

    time_t last_update_time; // time since sessions last updated

    char cmd_buf[FTP_CMDBUF_SIZE];
    size_t cmd_buf_size;

    char send_buf[FTP_SENDBUF_SIZE];
    size_t send_buf_offset;
    size_t send_buf_size;

    struct Pathname pwd;   // current directory
    struct Pathname temp_path; // rename from buffer / LIST fullpath
};

struct FtpCommand {
    const char name[5];
    void (*func)(struct FtpSession* session, const char* data);
    bool auth_required;
    bool args_required;
    bool data_connection_required;
};

struct Ftp {
    int initialised;
    struct FtpSocket server_sock;

    unsigned session_count;
    struct FtpSession sessions[FTP_MAX_SESSIONS];

    unsigned char data_buf[FTP_FILE_BUFFER_SIZE];
    struct FtpSrvConfig cfg;
};

static struct Ftp g_ftp = {0};

#if !HAVE_STRNCASECMP
static int strncasecmp(const char* a, const char* b, size_t len) {
    int rc = 0;
    for (size_t i = 0; i < len; i++) {
        if ((rc = tolower((unsigned char)a[i]) - tolower((unsigned char)b[i]))) {
            break;
        }
    }
    return rc;
}
#endif

static struct tm* unpack_time(const time_t* timer, struct tm* buf) {
    struct tm* r;

    if (g_ftp.cfg.use_localtime) {
#if defined(HAVE_LOCALTIME_R) && HAVE_LOCALTIME_R
        r = localtime_r(timer, buf);
#else
        r = localtime(timer);
#endif
    } else {
#if defined(HAVE_GMTIME_R) && HAVE_GMTIME_R
        r = gmtime_r(timer, buf);
#else
        r = gmtime(timer);
#endif
    }

    if (r) {
        *buf = *r;
    }

    return r;
}

static size_t ftp_get_timestamp_ms(void) {
    struct timeval ts;
    gettimeofday(&ts, NULL);
    return (ts.tv_sec * 1000000UL + ts.tv_usec) / 1000UL;
}

static void ftp_log_callback(enum FTP_API_LOG_TYPE type, const char* msg) {
    if (g_ftp.cfg.log_callback) {
        g_ftp.cfg.log_callback(type, msg);
    }
}

static void ftp_set_server_socket_options(struct FtpSocket* sock) {
    ftp_socket_set_nonblocking_enable(sock, 1);
    ftp_socket_set_reuseaddr_enable(sock, 1);
    ftp_socket_set_nodelay_enable(sock, 1);
    ftp_socket_set_keepalive_enable(sock, 1);
}

static void ftp_set_data_socket_options(struct FtpSocket* sock) {
    ftp_socket_set_data_buffer(sock);
    ftp_socket_set_nonblocking_enable(sock, 1);
    ftp_socket_set_keepalive_enable(sock, 1);
    ftp_socket_set_throughput_enable(sock, 1);
}

static inline unsigned socket_bind_port(void) {
    static unsigned port = 49152;
    const unsigned ret = port;
    port = (port == 65535) ? 49152 : port + 1;
    return ret;
}

// removes dangling '/' and duplicate '/' and converts '\\' to '/'
static void remove_slashes(struct Pathname* pathname) {
    int match = 0;
    size_t offset = 0;
    size_t len = strlen(pathname->s);

    while (offset < len) {
        if (pathname->s[offset] == '\\') {
            pathname->s[offset] = '/';
        }

        if (pathname->s[offset] == '/') {
            if (match == 1) {
                memmove(pathname->s + offset, pathname->s + offset + 1, len - offset);
                len--;
            } else {
                match = 1;
                offset++;
            }
        } else {
            match = 0;
            offset++;
        }
    }

    if (len > 1 && pathname->s[len - 1] == '/') {
        pathname->s[len - 1] = '\0';
    }
}

static int build_fullpath(const struct FtpSession* session, struct Pathname* out, struct Pathname pathname) {
    int rc = 0;
    remove_slashes(&pathname);

    // check if it's the fullpath
    if (pathname.s[0] == '/') {
        strcpy(out->s, pathname.s);
    } else if (!strcmp("..", pathname.s)) {
        strcpy(out->s, session->pwd.s);

        char* last_slash = strrchr(out->s, '/');
        if (!last_slash || last_slash == out->s) {
            strcpy(out->s, "/");
        } else {
            if (strlen(last_slash) > 1) {
                last_slash[0] = '\0';
            }
        }
    } else {
        if (session->pwd.s[strlen(session->pwd.s) - 1] != '/') {
            rc = snprintf(out->s, sizeof(*out), "%s/%s", session->pwd.s, pathname.s);
        } else {
            rc = snprintf(out->s, sizeof(*out), "%s%s", session->pwd.s, pathname.s);
        }
    }

    // return an error if the output was truncated or it failed.
    if (rc < 0 || rc >= sizeof(*out)) {
        rc = -1;
    } else {
        rc = 0;
    }

    return rc;
}

static void ftp_update_session_time(struct FtpSession* session) {
    if (session->state != FTP_SESSION_STATE_NONE) {
        session->last_update_time = time(NULL);
    }
}

// SOURCE: https://cr.yp.to/ftp/list/binls.html
static int ftp_build_list_entry(struct FtpSession* session, const struct Pathname* fullpath, const char* name, const struct stat* st) {
    int rc;
    struct FtpTransfer* transfer = &session->transfer;

    if (transfer->mode == FTP_TRANSFER_MODE_NLST) {
        rc = snprintf(transfer->list_buf, sizeof(transfer->list_buf), "%s" TELNET_EOL, name);
    } else {
        static const char months[12][4] = {
            "Jan", "Feb", "Mar", "Apr", "May", "Jun",
            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
        };

        char perms[11] = {0};
        switch (st->st_mode & S_IFMT) {
            case S_IFREG:   perms[0] = '-'; break;
            case S_IFDIR:   perms[0] = 'd'; break;
            case S_IFLNK:   perms[0] = 'l'; break;
            case S_IFIFO:   perms[0] = 'p'; break;
            case S_IFSOCK:  perms[0] = 's'; break;
            case S_IFCHR:   perms[0] = 'c'; break;
            case S_IFBLK:   perms[0] = 'b'; break;
            default:        perms[0] = '?'; break;
        }

        perms[1] = (st->st_mode & S_IRUSR) ? 'r' : '-';
        perms[2] = (st->st_mode & S_IWUSR) ? 'w' : '-';
        perms[3] = (st->st_mode & S_IXUSR) ? 'x' : '-';
        perms[4] = (st->st_mode & S_IRGRP) ? 'r' : '-';
        perms[5] = (st->st_mode & S_IWGRP) ? 'w' : '-';
        perms[6] = (st->st_mode & S_IXGRP) ? 'x' : '-';
        perms[7] = (st->st_mode & S_IROTH) ? 'r' : '-';
        perms[8] = (st->st_mode & S_IWOTH) ? 'w' : '-';
        perms[9] = (st->st_mode & S_IXOTH) ? 'x' : '-';

        struct Pathname symlink_path = {0};
        if (perms[0] == 'l') {
            strcpy(symlink_path.s, " -> ");
            const int len = ftp_vfs_readlink(fullpath->s, symlink_path.s + strlen(symlink_path.s), sizeof(symlink_path) - strlen(symlink_path.s));
            if (len < 0) {
                symlink_path.s[0] = '\0';
            }
        }

        struct tm tm = {0};
        unpack_time(&st->st_mtime, &tm);

        // if the time is greater than 6 months, show year rather than time
        char date[6] = {0};
        const long six_months = 60ll * 60ll * 24ll * (365ll / 2ll);
        if (labs((long)difftime(session->last_update_time, st->st_mtime)) > six_months) {
            snprintf(date, sizeof(date), "%5u", tm.tm_year + 1900);
        } else {
            snprintf(date, sizeof(date), "%02u:%02u", tm.tm_hour, tm.tm_min);
        }

        const unsigned nlink = st->st_nlink;
        const size_t size = S_ISDIR(st->st_mode) ? 0 : st->st_size;

        rc = snprintf(transfer->list_buf, sizeof(transfer->list_buf), "%s %3u %s %s %13zu %s %3d %s %s%s" TELNET_EOL,
            perms,
            nlink,
            ftp_vfs_getpwuid(st), ftp_vfs_getgrgid(st),
            size,
            months[tm.tm_mon], tm.tm_mday, date,
            name, symlink_path.s);
    }

    // don't send anything on error or truncated
    if (rc <= 0 || rc > sizeof(transfer->list_buf)) {
        rc = -1;
    } else {
        transfer->size = rc;
    }

    return rc;
}

static int ftp_build_mlst_entry(const char* name, const struct stat* st, char* out, int* out_len) {
    int rc;

    struct tm tm = {0};
    unpack_time(&st->st_mtime, &tm);

    const char* type = "file";
    if (S_ISDIR(st->st_mode)) {
        type = "dir";
    } else if (S_ISLNK(st->st_mode)) {
        type = "link";
    }

    char modify[16] = {0};
    snprintf(modify, sizeof(modify), "%04u%02u%02u%02u%02u%02u",
        st->st_mtime ? (tm.tm_year + 1900) : 1970,
        st->st_mtime ? (tm.tm_mon + 1) : 1,
        st->st_mtime ? tm.tm_mday : 1,
        st->st_mtime ? tm.tm_hour : 0,
        st->st_mtime ? tm.tm_min : 0,
        st->st_mtime ? tm.tm_sec : 0
    );

    char perms[16] = {0};
    const bool writeable = st->st_mode & S_IWUSR;
    const bool is_dir = S_ISDIR(st->st_mode);

    if (is_dir) {
        if (writeable) {
            strcpy(perms, "cdeflmp");
        } else {
            strcpy(perms, "el");
        }
    } else {
        if (writeable) {
            strcpy(perms, "adfrw");
        } else {
            strcpy(perms, "r");
        }
    }

    // send the header
    rc = snprintf(out, *out_len, "Type=%s;Size=%zu;Modify=%s;Perm=%s;UNIX.mode=%04o;UNIX.owner=%d;UNIX.group=%d; %s" TELNET_EOL,
        type,
        (size_t)st->st_size,
        modify,
        perms,
        st->st_mode & ALLPERMS, // only the permission bits
        st->st_uid,
        st->st_gid,
        name
    );

    // don't send anything on error or truncated
    if (rc <= 0 || rc > *out_len) {
        rc = -1;
    } else {
        *out_len = rc;
    }

    return rc;
}

static void ftp_session_send(struct FtpSession* session);

static void ftp_client_msg(struct FtpSession* session, unsigned code, const char* fmt, ...) {
    // prepend with the code.
    const size_t size = sizeof(session->send_buf);
    const size_t code_len = snprintf(session->send_buf, size, "%u ", code);
    const size_t eol_padding = code_len * 2 + 4 + 3;

    // append message.
    va_list va;
    va_start(va, fmt);
    vsnprintf(session->send_buf + code_len, size - eol_padding, fmt, va);
    va_end(va);

    // if multiline message, append END.
    const size_t len = strlen(session->send_buf);
    if (len > code_len && session->send_buf[code_len] == '-') {
        memmove(session->send_buf + code_len - 1, session->send_buf + code_len, len - code_len);
        snprintf(session->send_buf + len - 1, size - len - 3, "%d END", code);
    }

    if (code < 400) {
        ftp_log_callback(FTP_API_LOG_TYPE_RESPONSE, session->send_buf);
    } else {
        ftp_log_callback(FTP_API_LOG_TYPE_ERROR, session->send_buf);
    }

    // finally, append EOL and send message.
    strcat(session->send_buf, TELNET_EOL);
    session->send_buf_offset = 0;
    session->send_buf_size = strlen(session->send_buf);
    session->state = FTP_SESSION_STATE_POLLOUT;

    // try to send immediately.
    ftp_session_send(session);
}

static void ftp_data_transfer_end(struct FtpSession* session) {
    switch (session->data_connection) {
        case FTP_DATA_CONNECTION_NONE:
            break;
        case FTP_DATA_CONNECTION_ACTIVE:
            ftp_socket_close(&session->data_sock);
            break;
        case FTP_DATA_CONNECTION_PASSIVE:
            ftp_socket_close(&session->data_sock);
            ftp_socket_close(&session->pasv_sock);
            break;
    }

    ftp_vfs_close(&session->transfer.file_vfs);
    ftp_vfs_closedir(&session->transfer.dir_vfs);

    session->transfer.connection_pending = false;
    session->temp_path.s[0] = '\0';
    session->transfer.offset = 0;
    session->transfer.size = 0;
    session->transfer.mode = FTP_TRANSFER_MODE_NONE;
    session->data_connection = FTP_DATA_CONNECTION_NONE;
}

static void ftp_data_poll(struct FtpSession* session) {
    int rc = 0;

    if (session->data_connection == FTP_DATA_CONNECTION_ACTIVE) {
        rc = ftp_socket_connect(&session->data_sock, (struct sockaddr*)&session->data_sockaddr, sizeof(session->data_sockaddr));
        if (rc < 0) {
            if (errno == EAGAIN || errno == EINPROGRESS || errno == EALREADY) {
                // blocking...
            } else if (errno == EISCONN) {
                session->transfer.connection_pending = false;
            } else {
                ftp_client_msg(session, 425, "Can't open data connection, [poll] %d %s.", errno, strerror(errno));
                ftp_data_transfer_end(session);
            }
        } else {
            session->transfer.connection_pending = false;
        }
    } else {
        size_t socklen = sizeof(session->pasv_sockaddr);
        rc = ftp_socket_accept(&session->data_sock, &session->pasv_sock, (struct sockaddr*)&session->pasv_sockaddr, &socklen);
        if (rc < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // blocking...
            } else {
                ftp_client_msg(session, 425, "Can't open data connection, [poll] %d %s.", errno, strerror(errno));
                ftp_data_transfer_end(session);
            }
        } else {
            ftp_set_data_socket_options(&session->data_sock);
            session->transfer.connection_pending = false;
        }
    }
}

static void ftp_data_open(struct FtpSession* session, enum FTP_TRANSFER_MODE mode) {
    int rc = 0;
    ftp_client_msg(session, 150, "File status okay; about to open data connection.");

    if (session->data_connection == FTP_DATA_CONNECTION_ACTIVE) {
        rc = ftp_socket_open(&session->data_sock, PF_INET, SOCK_STREAM, 0);
        if (rc >= 0) {
            ftp_set_data_socket_options(&session->data_sock);
        }
    }

    if (rc < 0) {
        ftp_client_msg(session, 425, "Can't open data connection [NORM], %s.", strerror(errno));
        ftp_data_transfer_end(session);
    } else {
        session->transfer.mode = mode;
        session->transfer.index = 0;
        session->transfer.connection_pending = true;

        // try to open immediately.
        ftp_data_poll(session);
    }
}

static enum FTP_FILE_TRANSFER_STATE ftp_dir_data_transfer_progress(struct FtpSession* session, struct FtpTransfer* transfer) {
    // send as much data as possible.
    if (transfer->size) {
        const int n = ftp_socket_send(&session->data_sock, transfer->list_buf + transfer->offset, transfer->size, 0);
        if (n < 0) {
            // check if it failed due to anything but blocking.
            if (errno != EWOULDBLOCK && errno != EAGAIN) {
                return FTP_FILE_TRANSFER_STATE_ERROR;
            } else {
                return FTP_FILE_TRANSFER_STATE_BLOCKING;
            }
        } else if (n != transfer->size) {
            // partial transfer.
            transfer->offset += n;
            transfer->size -= n;
            return FTP_FILE_TRANSFER_STATE_BLOCKING;
        } else {
            transfer->list_buf[0] = '\0';
            transfer->offset = 0;
            transfer->size = 0;

            // check if we are finished with this transfer.
            if (!ftp_vfs_isdir_open(&transfer->dir_vfs)) {
                return FTP_FILE_TRANSFER_STATE_FINISHED;
            }
        }
    } else {
        // parse the next file.
        static struct FtpVfsDirEntry entry;
        const char* name = ftp_vfs_readdir(&transfer->dir_vfs, &entry);
        if (!name) {
            return FTP_FILE_TRANSFER_STATE_FINISHED;
        }

        if (!strcmp(".", name) || !strcmp("..", name)) {
            return FTP_FILE_TRANSFER_STATE_CONTINUE;
        }

        int rc;
        struct Pathname filepath;
        if (session->temp_path.s[strlen(session->temp_path.s) - 1] != '/') {
            rc = snprintf(filepath.s, sizeof(filepath), "%s/%s", session->temp_path.s, name);
        } else {
            rc = snprintf(filepath.s, sizeof(filepath), "%s%s", session->temp_path.s, name);
        }

        if (rc <= 0 || rc >= sizeof(filepath)) {
            return FTP_FILE_TRANSFER_STATE_CONTINUE;
        }

        struct stat st = {0};
        rc = ftp_vfs_dirlstat(&transfer->dir_vfs, &entry, filepath.s, &st);
        if (rc < 0) {
            return FTP_FILE_TRANSFER_STATE_CONTINUE;
        }

        if (transfer->mode == FTP_TRANSFER_MODE_MLSD) {
            int len = sizeof(transfer->list_buf);
            ftp_build_mlst_entry(name, &st, transfer->list_buf, &len);
            transfer->size = len;
        } else {
            ftp_build_list_entry(session, &filepath, name, &st);
        }
    }

    return FTP_FILE_TRANSFER_STATE_CONTINUE;
}

static enum FTP_FILE_TRANSFER_STATE ftp_file_data_transfer_progress(struct FtpSession* session, struct FtpTransfer* transfer) {
    int n;

    if (transfer->mode == FTP_TRANSFER_MODE_RETR) {
        const int read = n = ftp_vfs_read(&transfer->file_vfs, g_ftp.data_buf, sizeof(g_ftp.data_buf));
        if (n < 0) {
            return FTP_FILE_TRANSFER_STATE_ERROR;
        } else if (n == 0) {
            return FTP_FILE_TRANSFER_STATE_FINISHED;
        } else {
            n = ftp_socket_send(&session->data_sock, g_ftp.data_buf, n, 0);
            if (n < 0) {
                if (errno == EWOULDBLOCK || errno == EAGAIN) {
                    ftp_vfs_seek(&transfer->file_vfs, g_ftp.data_buf, 0, transfer->offset);
                    return FTP_FILE_TRANSFER_STATE_BLOCKING;
                } else {
                    return FTP_FILE_TRANSFER_STATE_ERROR;
                }
            } else {
                transfer->offset += (size_t)n;
                if (n != read) {
                    ftp_vfs_seek(&transfer->file_vfs, g_ftp.data_buf, n, transfer->offset);
                    return FTP_FILE_TRANSFER_STATE_BLOCKING;
                }
            }
        }
    } else {
        n = ftp_socket_recv(&session->data_sock, g_ftp.data_buf, sizeof(g_ftp.data_buf), 0);
        if (n < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                return FTP_FILE_TRANSFER_STATE_BLOCKING;
            } else {
                return FTP_FILE_TRANSFER_STATE_ERROR;
            }
        } else if (n == 0) {
            return FTP_FILE_TRANSFER_STATE_FINISHED;
        } else {
            n = ftp_vfs_write(&transfer->file_vfs, g_ftp.data_buf, n);
            if (n < 0) {
                return FTP_FILE_TRANSFER_STATE_ERROR;
            } else {
                transfer->offset += n;
            }
        }
    }

    return FTP_FILE_TRANSFER_STATE_CONTINUE;
}

static void ftp_data_transfer_progress(struct FtpSession* session) {
    struct FtpTransfer* transfer = &session->transfer;
    enum FTP_FILE_TRANSFER_STATE state = FTP_FILE_TRANSFER_STATE_CONTINUE;
    const size_t start = ftp_get_timestamp_ms();

    while (state == FTP_FILE_TRANSFER_STATE_CONTINUE) {
        if (transfer->mode == FTP_TRANSFER_MODE_RETR || transfer->mode == FTP_TRANSFER_MODE_STOR) {
            state = ftp_file_data_transfer_progress(session, transfer);
        } else {
            state = ftp_dir_data_transfer_progress(session, transfer);
        }

        if (g_ftp.cfg.progress_callback) {
            g_ftp.cfg.progress_callback();
        }

        // break out if 1ms has elapsed as to not block for too long.
        if (ftp_get_timestamp_ms() - start >= 1) {
            break;
        }
    }

    if (state == FTP_FILE_TRANSFER_STATE_ERROR) {
        ftp_client_msg(session, 426, "Connection closed; transfer aborted, %s", strerror(errno));
        ftp_data_transfer_end(session);
    } else if (state == FTP_FILE_TRANSFER_STATE_FINISHED) {
        ftp_client_msg(session, 226, "Closing data connection.");
        ftp_data_transfer_end(session);
    }

    ftp_update_session_time(session);
}

// USER <SP> <username> <CRLF> | 230, 530, 500, 501, 421, 331, 332
static void ftp_cmd_USER(struct FtpSession* session, const char* data) {
    char username[128] = {0};
    int rc = snprintf(username, sizeof(username), "%s", data);

    if (rc <= 0 || rc >= sizeof(username)) {
        ftp_client_msg(session, 501, "Syntax error in parameters or arguments.");
    } else if (g_ftp.cfg.anon) {
        if (strcmp(username, "anonymous")) {
            ftp_client_msg(session, 530, "Not logged in.");
        } else {
            session->auth_mode = FTP_AUTH_MODE_VALID;
            ftp_client_msg(session, 230, "User logged in, proceed.");
        }
    } else if (strcmp(username, g_ftp.cfg.user)) {
        ftp_client_msg(session, 530, "Not logged in.");
    } else {
        session->auth_mode = FTP_AUTH_MODE_NEED_PASS;
        ftp_client_msg(session, 331, "User name okay, need password.");
    }
}

// PASS <SP> <password> <CRLF> | 230, 202, 530, 500, 501, 503, 421, 332
static void ftp_cmd_PASS(struct FtpSession* session, const char* data) {
    char password[128] = {0};
    int rc = snprintf(password, sizeof(password), "%s", data);

    if (rc <= 0 || rc >= sizeof(password)) {
        ftp_client_msg(session, 501, "Syntax error in parameters or arguments.");
    } else if (session->auth_mode != FTP_AUTH_MODE_NEED_PASS) {
        ftp_client_msg(session, 503, "Bad sequence of commands.");
    } else if (strcmp(password, g_ftp.cfg.pass)) {
        ftp_client_msg(session, 530, "Not logged in.");
    } else {
        session->auth_mode = FTP_AUTH_MODE_VALID;
        ftp_client_msg(session, 230, "User logged in, proceed.");
    }
}

// ACCT <SP> <account-information> <CRLF> | 230, 202, 530, 500, 501, 503, 421
static void ftp_cmd_ACCT(struct FtpSession* session, const char* data) {
    ftp_client_msg(session, 500, "Syntax error, command unrecognized.");
}

// used by CDUP and CWD
static void ftp_set_directory(struct FtpSession* session, const struct Pathname* pathname) {
    struct Pathname fullpath = {0};
    int rc = build_fullpath(session, &fullpath, *pathname);

    if (rc >= 0) {
        if (strcmp("/", fullpath.s)) {
            struct stat st = {0};
            rc = ftp_vfs_stat(fullpath.s, &st);
            if (!S_ISDIR(st.st_mode)) {
                errno = ENOTDIR;
                rc = -1;
            }
        }
    }

    if (rc < 0) {
        ftp_client_msg(session, 550, "Requested action not taken, %s. Bad path: %s.", strerror(errno), fullpath.s);
    } else {
        session->pwd = fullpath;
        ftp_client_msg(session, 200, "Command okay.");
    }
}

// CWD <SP> <pathname> <CRLF> | 250, 500, 501, 502, 421, 530, 550
static void ftp_cmd_CWD(struct FtpSession* session, const char* data) {
    struct Pathname pathname = {0};
    int rc = snprintf(pathname.s, sizeof(pathname), "%s", data);

    if (rc <= 0 || rc >= sizeof(pathname)) {
        ftp_client_msg(session, 501, "Syntax error in parameters or arguments.");
    } else {
        ftp_set_directory(session, &pathname);
    }
}

// CDUP <SP> <pathname> <CRLF> | 250, 500, 501, 502, 421, 530, 550
static void ftp_cmd_CDUP(struct FtpSession* session, const char* data) {
    if (!strcmp("/", session->pwd.s)) {
        ftp_client_msg(session, 550, "Requested action not taken.");
    } else {
        ftp_set_directory(session, &(const struct Pathname){".."});
    }
}

// SMNT <SP> <> <CRLF> | 202, 250, 500, 501, 502, 421, 530, 550
static void ftp_cmd_SMNT(struct FtpSession* session, const char* data) {
    ftp_client_msg(session, 500, "Syntax error, command unrecognized.");
}

// REIN <CRLF> | 120, 220, 220, 421, 500, 502
static void ftp_cmd_REIN(struct FtpSession* session, const char* data) {
    ftp_client_msg(session, 500, "Syntax error, command unrecognized.");
}

// QUIT <SP> <password> <CRLF> | 221, 500
static void ftp_cmd_QUIT(struct FtpSession* session, const char* data) {
    ftp_client_msg(session, 221, "Service closing control connection.");
}

// PORT <SP> <host-port> <CRLF> | 200, 500, 501, 421, 530
static void ftp_cmd_PORT(struct FtpSession* session, const char* data) {
    ftp_data_transfer_end(session);

    unsigned char h[6] = {0}; // ip addr / port
    for (int i = 0; i < 6; i++) {
        char* end_ptr;
        const unsigned long value = strtoul(data, &end_ptr, 10);
        if ((!value && data == end_ptr) || value > 255) {
            ftp_client_msg(session, 501, "Syntax error in parameters or arguments.");
            return;
        }

        h[i] = value;

        // skip over comma.
        if (end_ptr[0] != '\0') {
            data = end_ptr + 1;
        }
    }

    // convert ip string to sockaddr_in
    char ip_buf[16] = {0};
    int rc = snprintf(ip_buf, sizeof(ip_buf), "%u.%u.%u.%u", h[0], h[1], h[2], h[3]);
    if (rc <= 0 || rc >= sizeof(ip_buf)) {
        ftp_client_msg(session, 501, "Syntax error in parameters or arguments, %s.", strerror(errno));
    } else {
        rc = inet_aton(ip_buf, &session->data_sockaddr.sin_addr);
        if (rc < 0) {
            ftp_client_msg(session, 501, "Syntax error in parameters or arguments, %s.", strerror(errno));
        } else {
            session->data_sockaddr.sin_family = PF_INET;
            session->data_sockaddr.sin_port = htons((h[4] << 8) + h[5]);
            session->data_connection = FTP_DATA_CONNECTION_ACTIVE;
            ftp_client_msg(session, 200, "Command okay.");
        }
    }
}

// PASV <CRLF> | 227, 500, 501, 502, 421, 530
static void ftp_cmd_PASV(struct FtpSession* session, const char* data) {
    ftp_data_transfer_end(session);
    int rc = ftp_socket_open(&session->pasv_sock, PF_INET, SOCK_STREAM, 0);

    if (rc < 0) {
        ftp_client_msg(session, 501, "open failed Syntax error in parameters or arguments, %s.", strerror(errno));
    } else {
        ftp_set_server_socket_options(&session->pasv_sock);

        // copy current over ip addr
        session->pasv_sockaddr = session->control_sockaddr;
        session->pasv_sockaddr.sin_port = htons(socket_bind_port());

        rc = ftp_socket_bind(&session->pasv_sock, (struct sockaddr*)&session->pasv_sockaddr, sizeof(session->pasv_sockaddr));
        if (rc < 0) {
            ftp_client_msg(session, 501, "bind failed Syntax error in parameters or arguments, %s", strerror(errno));
        } else {
            rc = ftp_socket_listen(&session->pasv_sock, 1);
            if (rc < 0) {
                ftp_client_msg(session, 501, "listen failed Syntax error in parameters or arguments, %s.", strerror(errno));
            } else {
                size_t base_len = sizeof(session->pasv_sockaddr);
                rc = ftp_socket_getsockname(&session->pasv_sock, (struct sockaddr*)&session->pasv_sockaddr, &base_len);
                if (rc < 0) {
                    ftp_client_msg(session, 501, "socket_getsockname failed Syntax error in parameters or arguments, %s.", strerror(errno));
                } else {
                    char ip_buf[16] = {0};
                    const char* addr_s = inet_ntoa(session->control_sockaddr.sin_addr);
                    for (int i = 0; addr_s[i]; i++) {
                        ip_buf[i] = addr_s[i];
                        if (ip_buf[i] == '.') {
                            ip_buf[i] = ',';
                        }
                    }

                    const unsigned port = ntohs(session->pasv_sockaddr.sin_port);
                    session->data_connection = FTP_DATA_CONNECTION_PASSIVE;
                    ftp_client_msg(session, 227, "Entering Passive Mode (%s,%u,%u)", ip_buf, port >> 8, port & 0xFF);
                    return;
                }
            }
        }
        ftp_socket_close(&session->pasv_sock);
    }
}

// TYPE <SP> <type-code> <CRLF> | 200, 500, 501, 504, 421, 530
static void ftp_cmd_TYPE(struct FtpSession* session, const char* data) {
    const char code = data[0];

    if (code == '\0') {
        ftp_client_msg(session, 501, "Syntax error in parameters or arguments.");
    } else if (code == 'A') {
        session->type = FTP_TYPE_ASCII;
        ftp_client_msg(session, 200, "Command okay.");
    } else if (code == 'I') {
        session->type = FTP_TYPE_IMAGE;
        ftp_client_msg(session, 200, "Command okay.");
    } else {
        ftp_client_msg(session, 504, "Command not implemented for that parameter.");
    }
}

// STRU <SP> <structure-code> <CRLF> | 200, 500, 501, 504, 421, 530
static void ftp_cmd_STRU(struct FtpSession* session, const char* data) {
    const char code = data[0];

    if (code == '\0') {
        ftp_client_msg(session, 501, "Syntax error in parameters or arguments.");
    } else if (code == 'F') {
        session->structure = FTP_STRUCTURE_FILE;
        ftp_client_msg(session, 200, "Command okay.");
    } else {
        ftp_client_msg(session, 504, "Command not implemented for that parameter.");
    }
}

// MODE <SP> <mode-code> <CRLF> | 200, 500, 501, 504, 421, 530
static void ftp_cmd_MODE(struct FtpSession* session, const char* data) {
    const char code = data[0];

    if (code == '\0') {
        ftp_client_msg(session, 501, "Syntax error in parameters or arguments.");
    } else if (code == 'S') {
        session->mode = FTP_MODE_STREAM;
        ftp_client_msg(session, 200, "Command okay.");
    } else {
        ftp_client_msg(session, 504, "Command not implemented for that parameter.");
    }
}

static void ftp_open_file(struct FtpSession* session, const char* data, enum FtpVfsOpenMode open_mode, enum FTP_TRANSFER_MODE transfer_mode, int error_code) {
    session->transfer.offset = 0;
    if (session->server_marker) {
        session->transfer.offset = session->server_marker;
        session->server_marker = 0;
    }

    struct Pathname pathname = {0};
    int rc = snprintf(pathname.s, sizeof(pathname), "%s", data);

    if (rc <= 0 || rc >= sizeof(pathname)) {
        ftp_client_msg(session, 501, "Syntax error in parameters or arguments.");
    } else {
        struct Pathname fullpath = {0};
        rc = build_fullpath(session, &fullpath, pathname);
        if (rc < 0) {
            ftp_client_msg(session, error_code, "Requested action not taken.");
        } else {
            rc = ftp_vfs_open(&session->transfer.file_vfs, fullpath.s, open_mode);
            if (rc < 0) {
                ftp_client_msg(session, error_code, "Requested action not taken, %s Failed to open path: %s.", strerror(errno), fullpath.s);
            } else {
                if (session->transfer.offset) {
                    rc = ftp_vfs_seek(&session->transfer.file_vfs, NULL, 0, session->transfer.offset);
                }

                if (rc < 0) {
                    ftp_vfs_close(&session->transfer.file_vfs);
                    ftp_client_msg(session, 550, "Requested action not taken, %s. Failed to fseek path: %s", strerror(errno), fullpath.s);
                } else {
                    ftp_data_open(session, transfer_mode);
                }
            }
        }
    }
}

// RETR <SP> <pathname> <CRLF> | 125, 150, (110), 226, 250, 425, 426, 451, 450, 550, 500, 501, 421, 530
static void ftp_cmd_RETR(struct FtpSession* session, const char* data) {
    ftp_open_file(session, data, FtpVfsOpenMode_READ, FTP_TRANSFER_MODE_RETR, 550);
}

// STOR <SP> <pathname> <CRLF> | 125, 150, (110), 226, 250, 425, 426, 451, 551, 552, 532, 450, 452, 553, 500, 501, 421, 530
static void ftp_cmd_STOR(struct FtpSession* session, const char* data) {
    ftp_open_file(session, data, FtpVfsOpenMode_WRITE, FTP_TRANSFER_MODE_STOR, 551);
}

// APPE <SP> <pathname> <CRLF> | 125, 150, (110), 226, 250, 425, 426, 451, 551, 552, 532, 450, 550, 452, 553, 500, 501, 502, 421, 530
static void ftp_cmd_APPE(struct FtpSession* session, const char* data) {
    ftp_open_file(session, data, FtpVfsOpenMode_APPEND, FTP_TRANSFER_MODE_STOR, 551);
}

// ALLO <SP> <decimal-integer> <CRLF> | 200, 202, 500, 501, 504, 421, 530
static void ftp_cmd_ALLO(struct FtpSession* session, const char* data) {
    ftp_client_msg(session, 200, "Command okay.");
}

// REST <SP> <marker> <CRLF> | 500, 501, 502, 421, 530, 350
static void ftp_cmd_REST(struct FtpSession* session, const char* data) {
    char* end_ptr;
    const size_t server_marker = strtoul(data, &end_ptr, 10);

    if ((!server_marker && data == end_ptr) || server_marker == (size_t)-1) {
        ftp_client_msg(session, 501, "Syntax error in parameters or arguments.");
    } else {
        session->server_marker = server_marker;
        ftp_client_msg(session, 350, "Requested file action pending further information.");
    }
}

// RNFR <SP> <pathname> <CRLF> | 450, 550, 500, 501, 502, 421, 530, 350
static void ftp_cmd_RNFR(struct FtpSession* session, const char* data) {
    struct Pathname pathname = {0};
    int rc = snprintf(pathname.s, sizeof(pathname), "%s", data);

    if (rc <= 0 || rc >= sizeof(pathname)) {
        ftp_client_msg(session, 501, "Syntax error in parameters or arguments.");
    } else {
        rc = build_fullpath(session, &session->temp_path, pathname);
        if (rc < 0) {
            ftp_client_msg(session, 550, "Requested action not taken, %s.", strerror(errno));
        } else {
            ftp_client_msg(session, 350, "Requested file action pending further information.");
        }
    }
}

// RNTO <SP> <pathname> <CRLF> | 250, 532, 553, 500, 501, 502, 503, 421, 530
static void ftp_cmd_RNTO(struct FtpSession* session, const char* data) {
    struct Pathname pathname = {0};
    int rc = snprintf(pathname.s, sizeof(pathname), "%s", data);

    if (rc <= 0 || rc >= sizeof(pathname)) {
        ftp_client_msg(session, 501, "Syntax error in parameters or arguments.");
    } else {
        if (session->temp_path.s[0] == '\0') {
            ftp_client_msg(session, 503, "Bad sequence of commands.");
        } else {
            struct Pathname dst_path;
            rc = build_fullpath(session, &dst_path, pathname);
            if (rc < 0) {
                ftp_client_msg(session, 553, "Requested action not taken, %s.", strerror(errno));
            } else {
                rc = ftp_vfs_rename(session->temp_path.s, dst_path.s);
                if (rc < 0) {
                    ftp_client_msg(session, 553, "Requested action not taken, %s.", strerror(errno));
                } else {
                    ftp_client_msg(session, 250, "Requested file action okay, completed.");
                }
            }
        }
    }

    session->temp_path.s[0] = '\0';
}

// ABOR <CRLF> | 225, 226, 500, 501, 502, 421
static void ftp_cmd_ABOR(struct FtpSession* session, const char* data) {
    if (session->data_connection == FTP_DATA_CONNECTION_NONE) {
        ftp_client_msg(session, 226, "Closing data connection.");
    } else {
        if (session->transfer.mode == FTP_TRANSFER_MODE_NONE) {
            ftp_data_transfer_end(session);
            ftp_client_msg(session, 225, "Data connection open; no transfer in progress.");
        } else {
            ftp_data_transfer_end(session);
            ftp_client_msg(session, 426, "Connection closed; transfer aborted.");
            ftp_client_msg(session, 226, "Closing data connection.");
        }
    }
}

// used by DELE and RMD
static void ftp_remove_file(struct FtpSession* session, const char* data, int (*func)(const char*)) {
    struct Pathname pathname = {0};
    int rc = snprintf(pathname.s, sizeof(pathname), "%s", data);

    if (rc <= 0 || rc >= sizeof(pathname)) {
        ftp_client_msg(session, 501, "Syntax error in parameters or arguments.");
    } else {
        struct Pathname fullpath = {0};
        rc = build_fullpath(session, &fullpath, pathname);
        if (rc < 0) {
            ftp_client_msg(session, 550, "Requested action not taken, %s.", strerror(errno));
        } else {
            rc = func(fullpath.s);
            if (rc < 0) {
                ftp_client_msg(session, 550, "Requested action not taken, %s.", strerror(errno));
            } else {
                ftp_client_msg(session, 250, "Requested file action okay, completed.");
            }
        }
    }
}

// DELE <SP> <pathname> <CRLF> | 250, 450, 550, 500, 501, 502, 421, 530
static void ftp_cmd_DELE(struct FtpSession* session, const char* data) {
    ftp_remove_file(session, data, ftp_vfs_unlink);
}

// RMD  <SP> <pathname> <CRLF> | 250, 500, 501, 502, 421, 530, 550
static void ftp_cmd_RMD(struct FtpSession* session, const char* data) {
    ftp_remove_file(session, data, ftp_vfs_rmdir);
}

// MKD  <SP> <pathname> <CRLF> | 257, 500, 501, 502, 421, 530, 550
static void ftp_cmd_MKD(struct FtpSession* session, const char* data) {
    struct Pathname pathname = {0};
    int rc = snprintf(pathname.s, sizeof(pathname), "%s", data);

    if (rc <= 0 || rc >= sizeof(pathname)) {
        ftp_client_msg(session, 501, "Syntax error in parameters or arguments.");
    } else {
        struct Pathname fullpath = {0};
        rc = build_fullpath(session, &fullpath, pathname);
        if (rc < 0) {
            ftp_client_msg(session, 550, "Requested action not taken, %s.", strerror(errno));
        } else {
            rc = ftp_vfs_mkdir(fullpath.s);
            if (rc < 0) {
                ftp_client_msg(session, 550, "Requested action not taken, %s.", strerror(errno));
            } else {
                ftp_client_msg(session, 257, "\"%s\" created.", fullpath.s);
            }
        }
    }
}

// PWD  <CRLF> | 257, 500, 501, 502, 421, 550
static void ftp_cmd_PWD(struct FtpSession* session, const char* data) {
    ftp_client_msg(session, 257, "\"%s\" opened.", session->pwd.s);
}

// used by LIST and NLIST
static void ftp_list_directory(struct FtpSession* session, const char* data, enum FTP_TRANSFER_MODE mode) {
    struct Pathname pathname = {0};
    int rc = snprintf(pathname.s, sizeof(pathname), "%s", data);

    // NOTE: if breaking RFC wasn't enough, clients have started sending -a and -la
    // with trailing spaces...brilliant.
    if (rc == 0 || !strncmp("-a", pathname.s, strlen("-a")) || !strncmp("-la", pathname.s, strlen("-la"))) {
        session->temp_path = session->pwd;
    } else {
        rc = build_fullpath(session, &session->temp_path, pathname);
    }

    if (rc < 0 || rc >= sizeof(pathname)) {
        ftp_client_msg(session, 501, "Syntax error in parameters or arguments.");
    } else {
        struct stat st = {0};
        rc = ftp_vfs_lstat(session->temp_path.s, &st);
        if (rc < 0) {
            ftp_client_msg(session, 450, "Requested file action not taken. %s. Failed to stat path: %s.", strerror(errno), session->temp_path.s);
        } else {
            if (S_ISDIR(st.st_mode)) {
                rc = ftp_vfs_opendir(&session->transfer.dir_vfs, session->temp_path.s);
                if (rc < 0) {
                    ftp_client_msg(session, 450, "Requested file action not taken. %s. Failed to open dir: %s.", strerror(errno), session->temp_path.s);
                } else {
                    ftp_data_open(session, mode);
                }
            } else if (mode == FTP_TRANSFER_MODE_LIST) {
                rc = ftp_build_list_entry(session, &session->temp_path, pathname.s, &st);
                if (rc < 0) {
                    ftp_client_msg(session, 450, "Requested file action not taken, %s. Failed to build entry: %s.", strerror(errno), session->temp_path.s);
                } else {
                    ftp_data_open(session, mode);
                }
            } else if (mode == FTP_TRANSFER_MODE_MLSD) {
                int len = sizeof(session->transfer.list_buf);
                rc = ftp_build_mlst_entry(pathname.s, &st, session->transfer.list_buf, &len);
                if (rc < 0) {
                    ftp_client_msg(session, 450, "Requested file action not taken, %s. Failed to build entry: %s.", strerror(errno), session->temp_path.s);
                } else {
                    session->transfer.size = len;
                    ftp_data_open(session, mode);
                }
            } else {
                ftp_client_msg(session, 450, "Requested file action not taken. Nlist on file is not valid.");
            }
        }
    }
}

// LIST [<SP> <pathname>] <CRLF> | 125, 150, 226, 250, 425, 426, 451, 450, 500, 501, 502, 421, 530
static void ftp_cmd_LIST(struct FtpSession* session, const char* data) {
    ftp_list_directory(session, data, FTP_TRANSFER_MODE_LIST);
}

// NLST [<SP> <pathname>] <CRLF> | 125, 150, 226, 250, 425, 426, 451, 450, 500, 501, 502, 421, 530
static void ftp_cmd_NLST(struct FtpSession* session, const char* data) {
    ftp_list_directory(session, data, FTP_TRANSFER_MODE_NLST);
}

// SITE [<SP> <string>] <CRLF> | 200, 202, 500, 501, 530
static void ftp_cmd_SITE(struct FtpSession* session, const char* data) {
    ftp_client_msg(session, 500, "Syntax error, command unrecognized.");
}

// SYST <CRLF> | 215, 500, 501, 502, 421
static void ftp_cmd_SYST(struct FtpSession* session, const char* data) {
    ftp_client_msg(session, 215, "UNIX Type: L8");
}

// STAT [<SP> <string>] <CRLF> | 211, 212, 213, 450, 500, 501, 502, 421, 530
static void ftp_cmd_STAT(struct FtpSession* session, const char* data) {
    ftp_client_msg(session, 500, "Syntax error, command unrecognized.");
}

// HELP <CRLF> | 211, 214, 500, 501, 502, 421
static void ftp_cmd_HELP(struct FtpSession* session, const char* data) {
    ftp_client_msg(session, 214, "ftpsrv " FTPSRV_VERSION_STR " By TotalJustice.");
}

// NOOP <CRLF> | 200, 500, 421
static void ftp_cmd_NOOP(struct FtpSession* session, const char* data) {
    ftp_client_msg(session, 200, "Command okay.");
}

// FEAT <CRLF> | 211, 550
static void ftp_cmd_FEAT(struct FtpSession* session, const char* data) {
    ftp_client_msg(session, 211,
        "-Extensions supported:" TELNET_EOL
        " MDTM" TELNET_EOL
        " SIZE" TELNET_EOL
        " TVFS" TELNET_EOL
        " UTF8" TELNET_EOL
        " MLST Type*;Size*;Modify*;Perm*;UNIX.mode*;UNIX.owner;UNIX.group;" TELNET_EOL
    );
}

// MLST <CRLF> | 211, 550
static void ftp_cmd_MLST(struct FtpSession* session, const char* data) {
    struct Pathname pathname = {0};
    int rc = snprintf(pathname.s, sizeof(pathname), "%s", data);

    // NOTE: if breaking RFC wasn't enough, clients have started sending -a and -la
    // with trailing spaces...brilliant.
    if (rc == 0 || !strncmp("-a", pathname.s, strlen("-a")) || !strncmp("-la", pathname.s, strlen("-la"))) {
        session->temp_path = session->pwd;
    } else {
        rc = build_fullpath(session, &session->temp_path, pathname);
    }

    if (rc < 0 || rc >= sizeof(pathname)) {
        ftp_client_msg(session, 501, "Syntax error in parameters or arguments.");
    } else {
        struct stat st = {0};
        rc = ftp_vfs_lstat(session->temp_path.s, &st);
        if (rc < 0) {
            ftp_client_msg(session, 450, "Requested file action not taken. %s. Failed to stat path: %s.", strerror(errno), session->temp_path.s);
        } else {
            char buf[1024] = {0};
            int len = sizeof(buf);
            rc = ftp_build_mlst_entry(pathname.s, &st, buf, &len);
            if (rc < 0) {
                ftp_client_msg(session, 450, "Requested file action not taken, %s. Failed to build entry: %s.", strerror(errno), session->temp_path.s);
            } else {
                ftp_client_msg(session, 250, "- Listing" TELNET_EOL " %s", buf);
            }
        }
    }
}

// MLSD <CRLF> | 211, 550
static void ftp_cmd_MLSD(struct FtpSession* session, const char* data) {
    ftp_list_directory(session, data, FTP_TRANSFER_MODE_MLSD);
}

// OPTS <SP> <opts> <CRLF> | 200, 501
static void ftp_cmd_OPTS(struct FtpSession* session, const char* data) {
    if (!strcasecmp(data, "UTF8 ON")) {
        ftp_client_msg(session, 200, "Command okay.");
    } else if (!strcasecmp(data, "UTF8 OFF")) {
        ftp_client_msg(session, 200, "Command okay.");
    } else if (!strcasecmp(data, "UTF8")) {
        ftp_client_msg(session, 200, "Command okay.");
    } else {
        ftp_client_msg(session, 501, "Syntax error in parameters or arguments. %s", data);
    }

    // todo: handle OPTS MLST to toggle facts.
}

static int ftp_get_stat(struct FtpSession* session, const char* data, struct Pathname* fullpath, struct stat* st) {
    struct Pathname pathname = {0};
    int rc = snprintf(pathname.s, sizeof(pathname), "%s", data);

    if (rc <= 0 || rc >= sizeof(pathname)) {
        rc = -1;
        ftp_client_msg(session, 501, "Syntax error in parameters or arguments.");
    } else {
        rc = build_fullpath(session, fullpath, pathname);
        if (rc < 0) {
            ftp_client_msg(session, 501, "Syntax error in parameters or arguments, %s.", strerror(errno));
        } else {
            rc = ftp_vfs_stat(fullpath->s, st);
            if (rc < 0) {
                ftp_client_msg(session, 550, "Requested action not taken, %s. Bad path: %s.", strerror(errno), fullpath->s);
            }
        }
    }

    return rc;
}

// SIZE <SP> <pathname> <CRLF> | 213, 501, 550
static void ftp_cmd_SIZE(struct FtpSession* session, const char* data) {
    struct stat st = {0};
    struct Pathname fullpath = {0};
    int rc = ftp_get_stat(session, data, &fullpath, &st);

    if (!rc) {
        ftp_client_msg(session, 213, "%zu", (size_t)st.st_size);
    }
}

// MDTM <SP> <pathname> <CRLF> | 200, 501
static void ftp_cmd_MDTM(struct FtpSession* session, const char* data) {
    struct stat st = {0};
    struct Pathname fullpath = {0};
    int rc = ftp_get_stat(session, data, &fullpath, &st);

    if (!rc) {
        struct tm tm = {0};
        if (!unpack_time(&st.st_mtime, &tm)) {
            ftp_client_msg(session, 550, "Syntax error in parameters or arguments, %s. Failed to get timestamp: %s", strerror(errno), fullpath.s);
        } else {
            ftp_client_msg(session, 213, "%04d%02d%02d%02d%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
        }
    }
}

static const struct FtpCommand FTP_COMMANDS[] = {
    // ACCESS CONTROL COMMANDS: https://datatracker.ietf.org/doc/html/rfc959#section-4
    { .name = "USER", .func = ftp_cmd_USER, .auth_required = 0, .args_required = 1, .data_connection_required = 0 },
    { .name = "PASS", .func = ftp_cmd_PASS, .auth_required = 0, .args_required = 1, .data_connection_required = 0 },
    { .name = "ACCT", .func = ftp_cmd_ACCT, .auth_required = 0, .args_required = 1, .data_connection_required = 0 },
    { .name = "CWD",  .func = ftp_cmd_CWD,  .auth_required = 1, .args_required = 1, .data_connection_required = 0 },
    { .name = "CDUP", .func = ftp_cmd_CDUP, .auth_required = 1, .args_required = 0, .data_connection_required = 0 },
    { .name = "SMNT", .func = ftp_cmd_SMNT, .auth_required = 1, .args_required = 1, .data_connection_required = 0 },
    { .name = "REIN", .func = ftp_cmd_REIN, .auth_required = 0, .args_required = 0, .data_connection_required = 0 },
    { .name = "QUIT", .func = ftp_cmd_QUIT, .auth_required = 0, .args_required = 0, .data_connection_required = 0 },

    // TRANSFER PARAMETER COMMANDS
    { .name = "PORT", .func = ftp_cmd_PORT, .auth_required = 1, .args_required = 1, .data_connection_required = 0 },
    { .name = "PASV", .func = ftp_cmd_PASV, .auth_required = 1, .args_required = 0, .data_connection_required = 0 },
    { .name = "TYPE", .func = ftp_cmd_TYPE, .auth_required = 1, .args_required = 1, .data_connection_required = 0 },
    { .name = "STRU", .func = ftp_cmd_STRU, .auth_required = 1, .args_required = 1, .data_connection_required = 0 },
    { .name = "MODE", .func = ftp_cmd_MODE, .auth_required = 1, .args_required = 1, .data_connection_required = 0 },

    // FTP SERVICE COMMANDS
    { .name = "RETR", .func = ftp_cmd_RETR, .auth_required = 1, .args_required = 1, .data_connection_required = 1 },
    { .name = "STOR", .func = ftp_cmd_STOR, .auth_required = 1, .args_required = 1, .data_connection_required = 1 },
    { .name = "APPE", .func = ftp_cmd_APPE, .auth_required = 1, .args_required = 1, .data_connection_required = 1 },
    { .name = "ALLO", .func = ftp_cmd_ALLO, .auth_required = 1, .args_required = 1, .data_connection_required = 0 },
    { .name = "REST", .func = ftp_cmd_REST, .auth_required = 1, .args_required = 1, .data_connection_required = 0 },
    { .name = "RNFR", .func = ftp_cmd_RNFR, .auth_required = 1, .args_required = 1, .data_connection_required = 0 },
    { .name = "RNTO", .func = ftp_cmd_RNTO, .auth_required = 1, .args_required = 1, .data_connection_required = 0 },
    { .name = "ABOR", .func = ftp_cmd_ABOR, .auth_required = 0, .args_required = 0, .data_connection_required = 0 },
    { .name = "DELE", .func = ftp_cmd_DELE, .auth_required = 1, .args_required = 1, .data_connection_required = 0 },
    { .name = "RMD",  .func = ftp_cmd_RMD,  .auth_required = 1, .args_required = 1, .data_connection_required = 0 },
    { .name = "MKD",  .func = ftp_cmd_MKD,  .auth_required = 1, .args_required = 1, .data_connection_required = 0 },
    { .name = "PWD",  .func = ftp_cmd_PWD,  .auth_required = 1, .args_required = 0, .data_connection_required = 0 },
    { .name = "LIST", .func = ftp_cmd_LIST, .auth_required = 1, .args_required = 0, .data_connection_required = 1 },
    { .name = "NLST", .func = ftp_cmd_NLST, .auth_required = 1, .args_required = 0, .data_connection_required = 1 },
    { .name = "SITE", .func = ftp_cmd_SITE, .auth_required = 1, .args_required = 1, .data_connection_required = 0 },
    { .name = "SYST", .func = ftp_cmd_SYST, .auth_required = 0, .args_required = 0, .data_connection_required = 0 },
    { .name = "STAT", .func = ftp_cmd_STAT, .auth_required = 1, .args_required = 0, .data_connection_required = 0 },
    { .name = "HELP", .func = ftp_cmd_HELP, .auth_required = 0, .args_required = 0, .data_connection_required = 0 },
    { .name = "NOOP", .func = ftp_cmd_NOOP, .auth_required = 0, .args_required = 0, .data_connection_required = 0 },

    // extensions
    { .name = "FEAT", .func = ftp_cmd_FEAT, .auth_required = 0, .args_required = 0, .data_connection_required = 0 },
    // RFC 3659: https://datatracker.ietf.org/doc/html/rfc3659
    { .name = "MLST", .func = ftp_cmd_MLST, .auth_required = 1, .args_required = 0, .data_connection_required = 0 },
    { .name = "MLSD", .func = ftp_cmd_MLSD, .auth_required = 1, .args_required = 0, .data_connection_required = 1 },
    { .name = "SIZE", .func = ftp_cmd_SIZE, .auth_required = 1, .args_required = 1, .data_connection_required = 0 },
    { .name = "MDTM", .func = ftp_cmd_MDTM, .auth_required = 1, .args_required = 1, .data_connection_required = 0 },
    { .name = "OPTS", .func = ftp_cmd_OPTS, .auth_required = 0, .args_required = 1, .data_connection_required = 0 },
};

static int ftp_session_init(struct FtpSession* session) {
    struct sockaddr_in sa;
    size_t addr_len = sizeof(sa);
    memset(session, 0, sizeof(*session));
    session->control_sock.s = -1;
    session->data_sock.s = -1;
    session->pasv_sock.s = -1;

    int rc = ftp_socket_accept(&session->control_sock, &g_ftp.server_sock, (struct sockaddr*)&sa, &addr_len);
    if (rc < 0) {
        return rc;
    } else {
        ftp_set_server_socket_options(&session->control_sock);
        session->control_sockaddr = sa;
        addr_len = sizeof(session->control_sockaddr);

        rc = ftp_socket_getsockname(&session->control_sock, (struct sockaddr*)&session->control_sockaddr, &addr_len);
        if (rc < 0) {
            ftp_socket_close(&session->control_sock);
            ftp_client_msg(session, 451, "Failed to get connection info, %s.", strerror(errno));
            return rc;
        } else {
            session->state = FTP_SESSION_STATE_POLLIN;
            ftp_update_session_time(session);
            strcpy(session->pwd.s, "/");
            g_ftp.session_count++;
            ftp_client_msg(session, 220, "Service ready for new user.");
            return 0;
        }
    }
}

static void ftp_session_close(struct FtpSession* session) {
    if (session->state != FTP_SESSION_STATE_NONE) {
        ftp_data_transfer_end(session);
        ftp_socket_close(&session->control_sock);
        memset(session, 0, sizeof(*session));
        session->control_sock.s = -1;
        session->data_sock.s = -1;
        session->pasv_sock.s = -1;
        g_ftp.session_count--;
    }
}

static void ftp_session_progress_line(struct FtpSession* session, const char* line, size_t line_len) {
    char cmd_name[5] = {0};
    int rc = snprintf(cmd_name, sizeof(cmd_name), "%s", line);
    if (rc <= 0) {
        ftp_client_msg(session, 500, "Syntax error, command unrecognized.");
    } else {
        // correctly NULL cmd at the first space / TELNET_EOL
        for (int i = 0; cmd_name[i]; i++) {
            if (cmd_name[i] == ' ' || !strcmp(cmd_name + i, TELNET_EOL)) {
                cmd_name[i] = '\0';
                break;
            }
        }

        ftp_log_callback(FTP_API_LOG_TYPE_COMMAND, cmd_name);

        // find command and execute
        int command_id = -1;
        bool custom_command = false;
        for (size_t i = 0; i < FTP_ARR_SZ(FTP_COMMANDS); i++) {
            if (!strncasecmp(cmd_name, FTP_COMMANDS[i].name, sizeof(cmd_name))) {
                command_id = i;
                break;
            }
        }

        if (command_id < 0 && g_ftp.cfg.custom_command && g_ftp.cfg.custom_command_count) {
            for (size_t i = 0; i < g_ftp.cfg.custom_command_count; i++) {
                if (!strncasecmp(cmd_name, g_ftp.cfg.custom_command[i].name, sizeof(cmd_name))) {
                    custom_command = true;
                    command_id = i;
                    break;
                }
            }
        }

        if (command_id < 0) {
            ftp_client_msg(session, 500, "Syntax error, command \"%s\" unrecognized.", cmd_name);
        } else {
            if (custom_command) {
                const struct FtpSrvCustomCommand* cmd = &g_ftp.cfg.custom_command[command_id];
                const char* cmd_args = memchr(line + strlen(cmd->name), ' ', line_len - strlen(cmd->name));

                // validate the command
                if (cmd->args_required && !cmd_args) {
                    ftp_client_msg(session, 501, "Syntax error in parameters or arguments, missing required args.");
                } else if (cmd->auth_required && session->auth_mode != FTP_AUTH_MODE_VALID) {
                    ftp_client_msg(session, 530, "Not logged in.");
                } else {
                    const char* args = cmd_args ? cmd_args + 1 : "\0";
                    char msg_buf[FTP_SENDBUF_SIZE - 10] = {0};
                    const int code = cmd->func(cmd->userdata, args, msg_buf, sizeof(msg_buf));
                    ftp_client_msg(session, code, "%s", msg_buf);
                }
            } else {
                const struct FtpCommand* cmd = &FTP_COMMANDS[command_id];
                const char* cmd_args = memchr(line + strlen(cmd->name), ' ', line_len - strlen(cmd->name));

                // validate the command
                if (cmd->args_required && !cmd_args) {
                    ftp_client_msg(session, 501, "Syntax error in parameters or arguments, missing required args.");
                } else if (cmd->auth_required && session->auth_mode != FTP_AUTH_MODE_VALID) {
                    ftp_client_msg(session, 530, "Not logged in.");
                } else if (cmd->data_connection_required && session->data_connection == FTP_DATA_CONNECTION_NONE) {
                    ftp_client_msg(session, 501, "Syntax error in parameters or arguments, no data connection.");
                } else {
                    const char* args = cmd_args ? cmd_args + 1 : "\0";
                    cmd->func(session, args);
                }
            }
        }
    }
}

static void ftp_session_send(struct FtpSession* session) {
    int rc = ftp_socket_send(&session->control_sock, session->send_buf + session->send_buf_offset, session->send_buf_size - session->send_buf_offset, 0);
    if (rc < 0) {
        if (errno != EWOULDBLOCK && errno != EAGAIN) {
            ftp_session_close(session);
        }
    } else {
        session->send_buf_offset += rc;
        session->send_buf_size -= rc;

        if (!session->send_buf_size) {
            session->state = FTP_SESSION_STATE_POLLIN;
        }
    }

    ftp_update_session_time(session);
}

static void ftp_session_poll(struct FtpSession* session) {
    int rc = ftp_socket_recv(&session->control_sock, session->cmd_buf + session->cmd_buf_size, sizeof(session->cmd_buf) - session->cmd_buf_size, 0);
    if (rc < 0) {
        if (errno != EWOULDBLOCK && errno != EAGAIN) {
            ftp_session_close(session);
        }
    } else if (rc == 0) {
        ftp_session_close(session);
    } else {
        session->cmd_buf_size += rc;
        while (session->cmd_buf_size) {
            size_t line_len = 0;
            for (size_t i = 0; i < session->cmd_buf_size - 1; i++) {
                if (!memcmp(session->cmd_buf + i, TELNET_EOL, strlen(TELNET_EOL))) {
                    // replace TELNET_EOL with NULL as to terminate the string.
                    session->cmd_buf[i] = '\0';
                    line_len = i + strlen(TELNET_EOL);
                    break;
                }
            }

            if (!line_len) {
                // no room for TELNET_EOL, so reset the buffer.
                if (session->cmd_buf_size == sizeof(session->cmd_buf)) {
                    session->cmd_buf_size = 0;
                }
                break;
            }

            // consume line.
            ftp_session_progress_line(session, session->cmd_buf, line_len);
            memcpy(session->cmd_buf, session->cmd_buf + line_len, session->cmd_buf_size - line_len);
            session->cmd_buf_size -= line_len;
        }
    }

    ftp_update_session_time(session);
}

int ftpsrv_init(const struct FtpSrvConfig* cfg) {
    int rc;

    if (g_ftp.initialised || !cfg) {
        rc = -1;
    } else {
        memset(&g_ftp, 0, sizeof(g_ftp));
        memcpy(&g_ftp.cfg, cfg, sizeof(*cfg));
        g_ftp.initialised = 1;
        g_ftp.server_sock.s = -1;
        for (size_t i = 0; i < FTP_ARR_SZ(g_ftp.sessions); i++) {
            g_ftp.sessions[i].control_sock.s = -1;
            g_ftp.sessions[i].data_sock.s = -1;
            g_ftp.sessions[i].pasv_sock.s = -1;
        }

        rc = ftp_socket_open(&g_ftp.server_sock, PF_INET, SOCK_STREAM, 0);
        if (rc < 0) {
        } else {
            ftp_set_server_socket_options(&g_ftp.server_sock);

            struct sockaddr_in sa = {
                .sin_family = PF_INET,
                .sin_port = htons(cfg->port),
                .sin_addr.s_addr = INADDR_ANY,
            };

            rc = ftp_socket_bind(&g_ftp.server_sock, (struct sockaddr*)&sa, sizeof(sa));
            if (rc < 0) {
            } else {
                rc = ftp_socket_listen(&g_ftp.server_sock, 5); /* SOMAXCONN */
            }
        }
    }

    return rc;
}

int ftpsrv_loop(int timeout_ms) {
    if (!g_ftp.initialised) {
        return FTP_API_LOOP_ERROR_INIT;
    }

    // close all sessions that have expired.
    if (g_ftp.cfg.timeout) {
        const time_t cur_time = time(NULL);
        for (size_t i = 0; i < FTP_ARR_SZ(g_ftp.sessions); i++) {
            struct FtpSession* session = &g_ftp.sessions[i];
            if (session->state != FTP_SESSION_STATE_NONE) {
                if (difftime(cur_time, session->last_update_time) >= g_ftp.cfg.timeout) {
                    ftp_session_close(session);
                }
            }
        }
    }

    static struct FtpSocketPollEntry fds[1 + FTP_MAX_SESSIONS * 2] = {0};
    static struct FtpSocketPollFd poll_fds[1 + FTP_MAX_SESSIONS * 2] = {0};
    const size_t nfds = FTP_ARR_SZ(fds);

    // initialise fds.
    memset(fds, 0, sizeof(fds));

    // add server socket to the first entry.
    if (g_ftp.session_count < FTP_MAX_SESSIONS) {
        fds[0].fd = &g_ftp.server_sock;
        fds[0].events = FtpSocketPollType_IN;
    }

    // add each session control and data socket.
    for (size_t i = 0; i < FTP_ARR_SZ(g_ftp.sessions); i++) {
        const size_t si = 1 + i * 2;
        const size_t sd = 1 + i * 2 + 1;
        struct FtpSession* session = &g_ftp.sessions[i];

        if (session->state != FTP_SESSION_STATE_NONE) {
            if (session->state == FTP_SESSION_STATE_POLLIN) {
                fds[si].fd = &session->control_sock;
                fds[si].events = FtpSocketPollType_IN;
            } else if (session->state == FTP_SESSION_STATE_POLLOUT) {
                fds[si].fd = &session->control_sock;
                fds[si].events = FtpSocketPollType_OUT;
            }

            if (session->transfer.mode != FTP_TRANSFER_MODE_NONE) {
                // wait until the socket is ready to connect.
                if (session->transfer.connection_pending) {
                    if (session->data_connection == FTP_DATA_CONNECTION_PASSIVE) {
                        fds[sd].fd = &session->pasv_sock;
                        fds[sd].events = FtpSocketPollType_IN;
                    } else {
                        fds[sd].fd = &session->data_sock;
                        fds[sd].events = FtpSocketPollType_OUT;
                    }
                } else {
                    fds[sd].fd = &session->data_sock;
                    if (session->transfer.mode == FTP_TRANSFER_MODE_STOR) {
                        fds[sd].events = FtpSocketPollType_IN;
                    } else {
                        fds[sd].events = FtpSocketPollType_OUT;
                    }
                }
            }
        }
    }

    const int rc = ftp_socket_poll(fds, poll_fds, nfds, timeout_ms);
    if (rc < 0) {
        return FTP_API_LOOP_ERROR_INIT;
    } else {
        if (fds[0].revents & FtpSocketPollType_ERROR) {
            return FTP_API_LOOP_ERROR_INIT;
        } else if (fds[0].revents & FtpSocketPollType_IN) {
            for (size_t i = 0; i < FTP_ARR_SZ(g_ftp.sessions); i++) {
                if (g_ftp.sessions[i].state == FTP_SESSION_STATE_NONE) {
                    ftp_session_init(&g_ftp.sessions[i]);
                    break;
                }
            }
        }

        for (size_t i = 0; i < FTP_ARR_SZ(g_ftp.sessions); i++) {
            const size_t si = 1 + i * 2;
            const size_t sd = 1 + i * 2 + 1;
            struct FtpSession* session = &g_ftp.sessions[i];

            // if (fds[si].revents & (POLLERR | POLLHUP | POLLNVAL)) {
            if (fds[si].revents & FtpSocketPollType_ERROR) {
                ftp_session_close(session);
            } else if (fds[si].revents & FtpSocketPollType_IN) {
                ftp_session_poll(session);
            } else if (fds[si].revents & FtpSocketPollType_OUT) {
                ftp_session_send(session);
            }

            // don't close data transfer on error as it will confuse the client (ffmpeg)
            if (session->state != FTP_SESSION_STATE_NONE && session->transfer.mode != FTP_TRANSFER_MODE_NONE) {
                if (fds[sd].revents & (FtpSocketPollType_IN | FtpSocketPollType_OUT)) {
                    if (session->transfer.connection_pending) {
                        ftp_data_poll(session);
                    } else {
                        ftp_data_transfer_progress(session);
                    }
                }
            }
        }
    }

    return FTP_API_LOOP_ERROR_OK;
}

void ftpsrv_exit(void) {
    if (!g_ftp.initialised) {
        return;
    }

    for (size_t i = 0; i < FTP_ARR_SZ(g_ftp.sessions); i++) {
        if (g_ftp.sessions[i].state != FTP_SESSION_STATE_NONE) {
            ftp_session_close(&g_ftp.sessions[i]);
        }
    }

    ftp_socket_close(&g_ftp.server_sock);
    g_ftp.initialised = 0;
}
