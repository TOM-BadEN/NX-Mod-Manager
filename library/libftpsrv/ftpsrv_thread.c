/**
 * Based on ftpsrv by ITotalJustice
 * https://github.com/ITotalJustice/ftpsrv
 *
 * Thread wrapper for ftpsrv.
 * Manages a dedicated thread that runs ftpsrv_loop() in a loop.
 */
#include "ftpsrv_thread.h"
#include <switch.h>
#include <string.h>

static Thread s_thread;
static struct FtpSrvConfig s_config;
static volatile bool s_running = false;
static volatile bool s_shouldExit = false;

static void ftp_thread_func(void* arg) {
    (void)arg;
    while (!s_shouldExit) {
        int rc = ftpsrv_loop(500);
        if (rc != FTP_API_LOOP_ERROR_OK) {
            ftpsrv_exit();
            svcSleepThread(1000000000ULL);
            if (s_shouldExit) break;
            if (ftpsrv_init(&s_config) < 0) break;
        }
    }
    ftpsrv_exit();
    s_running = false;
}

int ftpsrv_start(const struct FtpSrvConfig* config) {
    if (s_running) return -1;

    s_config = *config;

    if (ftpsrv_init(&s_config) < 0) return -1;

    s_shouldExit = false;
    s_running = true;

    Result rc = threadCreate(&s_thread, ftp_thread_func, NULL, NULL, 1024 * 16, 0x31, -2);
    if (R_FAILED(rc)) {
        ftpsrv_exit();
        s_running = false;
        return -1;
    }

    if (R_FAILED(threadStart(&s_thread))) {
        threadClose(&s_thread);
        ftpsrv_exit();
        s_running = false;
        return -1;
    }

    return 0;
}

void ftpsrv_stop(void) {
    if (!s_running) return;
    s_shouldExit = true;
    threadWaitForExit(&s_thread);
    threadClose(&s_thread);
    s_running = false;
}

bool ftpsrv_is_running(void) {
    return s_running;
}
