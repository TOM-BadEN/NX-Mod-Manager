// Copyright 2024 TotalJustice.
// SPDX-License-Identifier: MIT
// https://github.com/ITotalJustice/ftpsrv
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stddef.h>
#include <poll.h>

#if defined(HAVE_IPTOS_THROUGHPUT) && HAVE_IPTOS_THROUGHPUT
    #include <netinet/ip.h>
#endif

struct FtpSocketPollFd {
    struct pollfd s;
};

struct FtpSocket {
    int s;
    int listening;
};

static inline int ftp_socket_open_nx(struct FtpSocket* sock, int domain, int type, int protocol) {
    sock->s = socket(domain, type, protocol);
    sock->listening = 0;
    return sock->s;
}

static inline int ftp_socket_recv_nx(struct FtpSocket* sock, void* buf, size_t size, int flags) {
    return recv(sock->s, buf, size, flags);
}

static inline int ftp_socket_send_nx(struct FtpSocket* sock, const void* buf, size_t size, int flags) {
    return send(sock->s, buf, size, flags);
}

static inline int ftp_socket_close_nx(struct FtpSocket* sock) {
    if (sock->s >= 0) {
        shutdown(sock->s, SHUT_RDWR);
        close(sock->s);
        sock->s = -1;
        sock->listening = 0;
    }
    return 0;
}

static inline int ftp_socket_accept_nx(struct FtpSocket* sock_out, struct FtpSocket* listen_sock, struct sockaddr* addr, size_t* addrlen) {
    socklen_t len = *addrlen;
    sock_out->s = accept(listen_sock->s, addr, &len);
    sock_out->listening = 0;
    *addrlen = len;
    return (sock_out->s >= 0) ? sock_out->s : -1;
}

static inline int ftp_socket_bind_nx(struct FtpSocket* sock, struct sockaddr* addr, size_t addrlen) {
    return bind(sock->s, addr, addrlen);
}

static inline int ftp_socket_connect_nx(struct FtpSocket* sock, struct sockaddr* addr, size_t addrlen) {
    return connect(sock->s, addr, addrlen);
}

static inline int ftp_socket_listen_nx(struct FtpSocket* sock, int backlog) {
    int rc = listen(sock->s, backlog);
    if (rc == 0) sock->listening = 1;
    return rc;
}

static inline int ftp_socket_getsockname_nx(struct FtpSocket* sock, struct sockaddr* addr, size_t* addrlen) {
    socklen_t len = *addrlen;
    int rc = getsockname(sock->s, addr, &len);
    *addrlen = len;
    return rc;
}

static inline int ftp_socket_set_reuseaddr_enable_nx(struct FtpSocket* sock, int enable) {
#if defined(HAVE_SO_REUSEADDR) && HAVE_SO_REUSEADDR
    const int option = 1;
    return setsockopt(sock->s, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
#else
    return 0;
#endif
}

static inline int ftp_socket_set_nodelay_enable_nx(struct FtpSocket* sock, int enable) {
#if defined(HAVE_TCP_NODELAY) && HAVE_TCP_NODELAY
    const int option = 1;
    return setsockopt(sock->s, IPPROTO_TCP, TCP_NODELAY, &option, sizeof(option));
#else
    return 0;
#endif
}

static inline int ftp_socket_set_keepalive_enable_nx(struct FtpSocket* sock, int enable) {
#if defined(HAVE_SO_KEEPALIVE) && HAVE_SO_KEEPALIVE
    const int option = 1;
    return setsockopt(sock->s, SOL_SOCKET, SO_KEEPALIVE, &option, sizeof(option));
#else
    return 0;
#endif
}

static inline int ftp_socket_set_throughput_enable_nx(struct FtpSocket* sock, int enable) {
#if defined(HAVE_IPTOS_THROUGHPUT) && HAVE_IPTOS_THROUGHPUT
    const int option = IPTOS_THROUGHPUT;
    return setsockopt(sock->s, IPPROTO_IP, IP_TOS, &option, sizeof(option));
#else
    return 0;
#endif
}

static inline int ftp_socket_set_data_buffer_nx(struct FtpSocket* sock) {
    return 0;
}

static inline int ftp_socket_set_nonblocking_enable_nx(struct FtpSocket* sock, int enable) {
    int flags = fcntl(sock->s, F_GETFL, 0);
    if (flags < 0) return -1;
    if (enable) flags |= O_NONBLOCK;
    else flags &= ~O_NONBLOCK;
    return fcntl(sock->s, F_SETFL, flags);
}

// libnx POSIX poll() wrapper does not reliably report POLLIN for
// listening sockets. Force POLLIN on listening sockets so that
// accept() is always attempted.
static inline int ftp_socket_poll_nx(struct FtpSocketPollEntry* entries, struct FtpSocketPollFd* _fds, size_t nfds, int timeout) {
    struct pollfd* fds = (struct pollfd*)_fds;

    for (size_t i = 0; i < nfds; i++) {
        entries[i].revents = 0;
        if (entries[i].fd && entries[i].fd->s >= 0) {
            fds[i].fd = entries[i].fd->s;
            fds[i].events = 0;
            fds[i].revents = 0;
            if (entries[i].events & FtpSocketPollType_IN) {
                fds[i].events |= POLLIN;
            }
            if (entries[i].events & FtpSocketPollType_OUT) {
                fds[i].events |= POLLOUT;
            }
        } else {
            fds[i].fd = -1;
            fds[i].events = 0;
            fds[i].revents = 0;
        }
    }

    int rc = poll(fds, nfds, timeout);
    if (rc < 0) {
        return rc;
    }

    for (size_t i = 0; i < nfds; i++) {
        if (entries[i].fd && entries[i].fd->s >= 0) {
            if (fds[i].revents & POLLIN) {
                entries[i].revents |= FtpSocketPollType_IN;
            }
            if (fds[i].revents & POLLOUT) {
                entries[i].revents |= FtpSocketPollType_OUT;
            }
            if (fds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
                entries[i].revents |= FtpSocketPollType_ERROR;
            }
            if (entries[i].fd->listening && (entries[i].events & FtpSocketPollType_IN)) {
                entries[i].revents |= FtpSocketPollType_IN;
                if (rc == 0) rc = 1;
            }
        }
    }

    return rc;
}

#define ftp_socket_open ftp_socket_open_nx
#define ftp_socket_recv ftp_socket_recv_nx
#define ftp_socket_send ftp_socket_send_nx
#define ftp_socket_close ftp_socket_close_nx
#define ftp_socket_accept ftp_socket_accept_nx
#define ftp_socket_bind ftp_socket_bind_nx
#define ftp_socket_connect ftp_socket_connect_nx
#define ftp_socket_listen ftp_socket_listen_nx
#define ftp_socket_getsockname ftp_socket_getsockname_nx
#define ftp_socket_set_reuseaddr_enable ftp_socket_set_reuseaddr_enable_nx
#define ftp_socket_set_nodelay_enable ftp_socket_set_nodelay_enable_nx
#define ftp_socket_set_keepalive_enable ftp_socket_set_keepalive_enable_nx
#define ftp_socket_set_throughput_enable ftp_socket_set_throughput_enable_nx
#define ftp_socket_set_data_buffer ftp_socket_set_data_buffer_nx
#define ftp_socket_set_nonblocking_enable ftp_socket_set_nonblocking_enable_nx
#define ftp_socket_poll ftp_socket_poll_nx

#ifdef __cplusplus
}
#endif
