# Based on ftpsrv by ITotalJustice
# https://github.com/ITotalJustice/ftpsrv

add_library(libftpsrv STATIC
    ftpsrv.c
    vfs_sd.c
    ftpsrv_thread.c
)

target_include_directories(libftpsrv PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(libftpsrv PRIVATE
    FTP_VFS_HEADER="vfs_sd.h"
    FTP_SOCKET_HEADER="socket_nx.h"
    FTP_MAX_SESSIONS=32
    FTP_FILE_BUFFER_SIZE=65536
    FTP_PATHNAME_SIZE=769
    FTPSRV_VERSION_MAJOR=1
    FTPSRV_VERSION_MINOR=2
    FTPSRV_VERSION_PATCH=2
    FTPSRV_VERSION_STR="1.2.2"
    HAVE_STRNCASECMP=1
    HAVE_LOCALTIME_R=0
    HAVE_GMTIME_R=0
    HAVE_SO_REUSEADDR=1
    HAVE_SO_KEEPALIVE=1
    HAVE_TCP_NODELAY=1
    HAVE_IPTOS_THROUGHPUT=0
)
