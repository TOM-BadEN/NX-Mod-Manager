/*
 * nxtc_utils.h
 *
 * Copyright (c) 2025, DarkMatterCore <pabloacurielz@gmail.com>.
 *
 * This file is part of libnxtc (https://github.com/DarkMatterCore/libnxtc).
 */

#pragma once

#ifndef __NXTC_UTILS_H__
#define __NXTC_UTILS_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <malloc.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <math.h>
#include <assert.h>
#include <nxtc_version.h>

#include "nxtc_log.h"

#define ALIGN_UP(x, y)          (((x) + ((y) - 1)) & ~((y) - 1))
#define IS_ALIGNED(x, y)        (((x) & ((y) - 1)) == 0)

#define IS_POWER_OF_TWO(x)      ((x) > 0 && ((x) & ((x) - 1)) == 0)

#define SCOPED_LOCK(mtx)        for(NxtcUtilsScopedLock scoped_lock __attribute__((__cleanup__(nxtcUtilsUnlockScope))) = nxtcUtilsLockScope(mtx); scoped_lock.cond; scoped_lock.cond = 0)

#define LIB_ASSERT(name, size)  static_assert(sizeof(name) == (size), "Bad size for " #name "! Expected " #size ".")

#define DEVOPTAB_SDMC_DEVICE    "sdmc:"

#define HBMENU_BASE_PATH        "/switch/"

/// Used by scoped locks.
typedef struct {
    Mutex *mtx;
    bool lock;
    int cond;
} NxtcUtilsScopedLock;

/// Commits SD card filesystem changes.
/// Must be used after closing a handle from a SD card file that has been written to.
bool nxtcUtilsCommitSdCardFileSystemChanges(void);

/// Returns a pointer to a dynamically allocated block with an address that's a multiple of 'alignment', which must be a power of two and a multiple of sizeof(void*).
/// The block size is guaranteed to be a multiple of 'alignment', even if 'size' isn't aligned to 'alignment'.
/// Returns NULL if an error occurs.
void *nxtcUtilsAlignedAlloc(size_t alignment, size_t size);

/// Trims whitespace characters from the provided string.
void nxtcUtilsTrimString(char *str);

/// Wrappers used in scoped locks.
NX_INLINE NxtcUtilsScopedLock nxtcUtilsLockScope(Mutex *mtx)
{
    NxtcUtilsScopedLock scoped_lock = { mtx, !mutexIsLockedByCurrentThread(mtx), 1 };
    if (scoped_lock.lock) mutexLock(scoped_lock.mtx);
    return scoped_lock;
}

NX_INLINE void nxtcUtilsUnlockScope(NxtcUtilsScopedLock *scoped_lock)
{
    if (scoped_lock->lock) mutexUnlock(scoped_lock->mtx);
}

#endif /* __NXTC_UTILS_H__ */
