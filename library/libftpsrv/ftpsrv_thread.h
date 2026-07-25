/**
 * Based on ftpsrv by ITotalJustice
 * https://github.com/ITotalJustice/ftpsrv
 *
 * Thread wrapper for ftpsrv.
 * Manages a dedicated thread that runs ftpsrv_loop() in a loop.
 */
#pragma once

#include "ftpsrv.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

int ftpsrv_start(const struct FtpSrvConfig* config);
void ftpsrv_stop(void);
bool ftpsrv_is_running(void);

#ifdef __cplusplus
}
#endif
