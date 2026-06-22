// Copyright 2024 TotalJustice.
// SPDX-License-Identifier: MIT
// https://github.com/ITotalJustice/ftpsrv
#ifndef FTP_SRV_SOCKET_H
#define FTP_SRV_SOCKET_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

enum FtpSocketPollType {
    FtpSocketPollType_IN = 1 << 0,
    FtpSocketPollType_OUT = 1 << 1,
    FtpSocketPollType_ERROR = 1 << 2,
};

struct FtpSocketPollEntry {
    struct FtpSocket* fd;
    enum FtpSocketPollType events;
    enum FtpSocketPollType revents;
};

struct FtpSocketPollFd;
struct FtpSocketLen;
struct FtpSocket;

struct sockaddr;
struct sockaddr_in;

int ftp_socket_open(struct FtpSocket* sock, int domain, int type, int protocol);
int ftp_socket_recv(struct FtpSocket* sock, void* buf, size_t size, int flags);
int ftp_socket_send(struct FtpSocket* sock, const void* buf, size_t size, int flags);
int ftp_socket_close(struct FtpSocket* sock);
int ftp_socket_accept(struct FtpSocket* sock_out, struct FtpSocket* listen_sock, struct sockaddr* addr, size_t* addrlen);
int ftp_socket_bind(struct FtpSocket* sock, struct sockaddr* addr, size_t addrlen);
int ftp_socket_connect(struct FtpSocket* sock, struct sockaddr* addr, size_t addrlen);
int ftp_socket_listen(struct FtpSocket* sock, int backlog);
int ftp_socket_getsockname(struct FtpSocket* sock, struct sockaddr* addr, size_t* addrlen);

// socket options
int ftp_socket_set_reuseaddr_enable(struct FtpSocket* sock, int enable);
int ftp_socket_set_nodelay_enable(struct FtpSocket* sock, int enable);
int ftp_socket_set_keepalive_enable(struct FtpSocket* sock, int enable);
int ftp_socket_set_throughput_enable(struct FtpSocket* sock, int enable);
int ftp_socket_set_nonblocking_enable(struct FtpSocket* sock, int enable);

// socket polling, may internally use select() if poll() is not available.
int ftp_socket_poll(struct FtpSocketPollEntry* entries, struct FtpSocketPollFd* fds, size_t nfds, int timeout);

#ifdef FTP_SOCKET_HEADER
    #include FTP_SOCKET_HEADER
#else
    #error FTP_SOCKET_HEADER not set to the header file path!
#endif

#ifdef __cplusplus
}
#endif

#endif // FTP_SRV_SOCKET_H
