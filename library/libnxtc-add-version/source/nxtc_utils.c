/*
 * nxtc_utils.c
 *
 * Copyright (c) 2025, DarkMatterCore <pabloacurielz@gmail.com>.
 *
 * This file is part of libnxtc (https://github.com/DarkMatterCore/libnxtc).
 */

#include "nxtc_utils.h"

FsFileSystem *g_sdCardFileSystem = NULL;

bool nxtcUtilsCommitSdCardFileSystemChanges(void)
{
    return (g_sdCardFileSystem ? R_SUCCEEDED(fsFsCommit(g_sdCardFileSystem)) : false);
}

void *nxtcUtilsAlignedAlloc(size_t alignment, size_t size)
{
    if (!alignment || !IS_POWER_OF_TWO(alignment) || (alignment % sizeof(void*)) != 0 || !size) return NULL;

    if (!IS_ALIGNED(size, alignment)) size = ALIGN_UP(size, alignment);

    return aligned_alloc(alignment, size);
}

void nxtcUtilsTrimString(char *str)
{
    size_t strsize = 0;
    char *start = NULL, *end = NULL;

    if (!str || !(strsize = strlen(str))) return;

    start = str;
    end = (start + strsize);

    while(--end >= start)
    {
        if (!isspace((unsigned char)*end)) break;
    }

    *(++end) = '\0';

    while(isspace((unsigned char)*start)) start++;

    if (start != str) memmove(str, start, end - start + 1);
}
