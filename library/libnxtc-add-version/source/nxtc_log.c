/*
 * nxtc_log.c
 *
 * Copyright (c) 2025, DarkMatterCore <pabloacurielz@gmail.com>.
 *
 * This file is part of libnxtc (https://github.com/DarkMatterCore/libnxtc).
 */

#ifdef DEBUG

#include "nxtc_utils.h"

#define LOG_FILE_NAME   "/" LIB_TITLE ".log"
#define LOG_BUF_SIZE    0x400000                /* 4 MiB. */
#define LOG_FORCE_FLUSH 0                       /* Forces a log buffer flush each time the logfile is written to. */

#define UTF8_BOM        "\xEF\xBB\xBF"
#define CRLF            "\r\n"

/* Global variables. */

static Mutex g_logMutex = 0;

static FsFile g_logFile = {0};
static s64 g_logFileOffset = 0;

static char *g_logBuffer = NULL;
static size_t g_logBufferLength = 0;

static const char *g_logStrFormat = "[%d-%02d-%02d %02d:%02d:%02d.%09lu] %s:%d:%s -> ";
static const char *g_logSessionSeparator = "________________________________________________________________\r\n";

extern FsFileSystem *g_sdCardFileSystem;

/* Function prototypes. */

static void _nxtcLogWriteStringToLogFile(const char *src);
static void _nxtcLogWriteFormattedStringToLogFile(const char *file_name, int line, const char *func_name, const char *fmt, va_list args);

static void _nxtcLogFlushLogFile(void);

static bool nxtcLogAllocateLogBuffer(void);
static bool nxtcLogOpenLogFile(void);

static void nxtcLogGenerateHexStringFromData(char *dst, size_t dst_size, const void *src, size_t src_size);

void nxtcLogWriteStringToLogFile(const char *src)
{
    SCOPED_LOCK(&g_logMutex) _nxtcLogWriteStringToLogFile(src);
}

__attribute__((format(printf, 4, 5))) void nxtcLogWriteFormattedStringToLogFile(const char *file_name, int line, const char *func_name, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    SCOPED_LOCK(&g_logMutex) _nxtcLogWriteFormattedStringToLogFile(file_name, line, func_name, fmt, args);
    va_end(args);
}

__attribute__((format(printf, 6, 7))) void nxtcLogWriteBinaryDataToLogFile(const void *data, size_t data_size, const char *file_name, int line, const char *func_name, const char *fmt, ...)
{
    if (!data || !data_size || !func_name || !*func_name || !fmt || !*fmt) return;

    va_list args;
    size_t data_str_size = ((data_size * 2) + 3);
    char *data_str = NULL;

    /* Allocate memory for the hex string representation of the provided binary data. */
    data_str = calloc(data_str_size, sizeof(char));
    if (!data_str) goto end;

    /* Generate hex string representation. */
    nxtcLogGenerateHexStringFromData(data_str, data_str_size, data, data_size);
    strcat(data_str, CRLF);

    SCOPED_LOCK(&g_logMutex)
    {
        /* Write formatted string. */
        va_start(args, fmt);
        _nxtcLogWriteFormattedStringToLogFile(file_name, line, func_name, fmt, args);
        va_end(args);

        /* Write hex string representation. */
        _nxtcLogWriteStringToLogFile(data_str);
    }

end:
    if (data_str) free(data_str);
}

void nxtcLogFlushLogFile(void)
{
    SCOPED_LOCK(&g_logMutex) _nxtcLogFlushLogFile();
}

void nxtcLogCloseLogFile(void)
{
    SCOPED_LOCK(&g_logMutex)
    {
        /* Flush log buffer. */
        _nxtcLogFlushLogFile();

        /* Close logfile. */
        if (serviceIsActive(&(g_logFile.s)))
        {
            fsFileClose(&g_logFile);
            memset(&g_logFile, 0, sizeof(FsFile));

            /* Commit SD card filesystem changes. */
            if (g_sdCardFileSystem) fsFsCommit(g_sdCardFileSystem);
        }

        /* Free log buffer. */
        if (g_logBuffer)
        {
            free(g_logBuffer);
            g_logBuffer = NULL;
        }

        /* Reset logfile offset. */
        g_logFileOffset = 0;
    }
}

static void _nxtcLogWriteStringToLogFile(const char *src)
{
    /* Make sure we have allocated memory for the log buffer and opened the logfile. */
    if (!src || !*src || !nxtcLogAllocateLogBuffer() || !nxtcLogOpenLogFile()) return;

    Result rc = 0;
    size_t src_len = strlen(src), tmp_len = 0;

    /* Check if the formatted string length is lower than the log buffer size. */
    if (src_len < LOG_BUF_SIZE)
    {
        /* Flush log buffer contents (if needed). */
        if ((g_logBufferLength + src_len) >= LOG_BUF_SIZE)
        {
            _nxtcLogFlushLogFile();
            if (g_logBufferLength) return;
        }

        /* Copy string into the log buffer. */
        strcpy(g_logBuffer + g_logBufferLength, src);
        g_logBufferLength += src_len;
    } else {
        /* Flush log buffer. */
        _nxtcLogFlushLogFile();
        if (g_logBufferLength) return;

        /* Write string data until it no longer exceeds the log buffer size. */
        while(src_len >= LOG_BUF_SIZE)
        {
            rc = fsFileWrite(&g_logFile, g_logFileOffset, src + tmp_len, LOG_BUF_SIZE, FsWriteOption_Flush);
            if (R_FAILED(rc)) return;

            g_logFileOffset += LOG_BUF_SIZE;
            tmp_len += LOG_BUF_SIZE;
            src_len -= LOG_BUF_SIZE;
        }

        /* Copy any remaining data from the string into the log buffer. */
        if (src_len)
        {
            strcpy(g_logBuffer, src + tmp_len);
            g_logBufferLength = src_len;
        }
    }

#if LOG_FORCE_FLUSH == 1
    /* Flush log buffer. */
    _nxtcLogFlushLogFile();
#endif
}

static void _nxtcLogWriteFormattedStringToLogFile(const char *file_name, int line, const char *func_name, const char *fmt, va_list args)
{
    /* Make sure we have allocated memory for the log buffer and opened the logfile. */
    if (!file_name || !*file_name || !func_name || !*func_name || !fmt || !*fmt || !nxtcLogAllocateLogBuffer() || !nxtcLogOpenLogFile()) return;

    Result rc = 0;

    int str1_len = 0, str2_len = 0;
    size_t log_str_len = 0;

    char *tmp_str = NULL;
    size_t tmp_len = 0;

    struct tm ts = {0};
    struct timespec now = {0};

    /* Get current time with nanosecond precision. */
    clock_gettime(CLOCK_REALTIME, &now);

    /* Get local time. */
    localtime_r(&(now.tv_sec), &ts);
    ts.tm_year += 1900;
    ts.tm_mon++;

    /* Get formatted string length. */
    str1_len = snprintf(NULL, 0, g_logStrFormat, ts.tm_year, ts.tm_mon, ts.tm_mday, ts.tm_hour, ts.tm_min, ts.tm_sec, now.tv_nsec, file_name, line, func_name);
    if (str1_len <= 0) return;

    str2_len = vsnprintf(NULL, 0, fmt, args);
    if (str2_len <= 0) return;

    log_str_len = (size_t)(str1_len + str2_len + 2);

    /* Check if the formatted string length is less than the log buffer size. */
    if (log_str_len < LOG_BUF_SIZE)
    {
        /* Flush log buffer contents (if needed). */
        if ((g_logBufferLength + log_str_len) >= LOG_BUF_SIZE)
        {
            _nxtcLogFlushLogFile();
            if (g_logBufferLength) return;
        }

        /* Nice and easy string formatting using the log buffer. */
        sprintf(g_logBuffer + g_logBufferLength, g_logStrFormat, ts.tm_year, ts.tm_mon, ts.tm_mday, ts.tm_hour, ts.tm_min, ts.tm_sec, now.tv_nsec, file_name, line, func_name);
        vsprintf(g_logBuffer + g_logBufferLength + (size_t)str1_len, fmt, args);
        strcat(g_logBuffer, CRLF);

        /* Update log buffer length. */
        g_logBufferLength += log_str_len;
    } else {
        /* Flush log buffer. */
        _nxtcLogFlushLogFile();
        if (g_logBufferLength) return;

        /* Allocate memory for a temporary buffer. This will hold the formatted string. */
        tmp_str = calloc(log_str_len + 1, sizeof(char));
        if (!tmp_str) return;

        /* Generate formatted string. */
        sprintf(tmp_str, g_logStrFormat, ts.tm_year, ts.tm_mon, ts.tm_mday, ts.tm_hour, ts.tm_min, ts.tm_sec, now.tv_nsec, file_name, line, func_name);
        vsprintf(tmp_str + (size_t)str1_len, fmt, args);
        strcat(tmp_str, CRLF);

        /* Write formatted string data until it no longer exceeds the log buffer size. */
        while(log_str_len >= LOG_BUF_SIZE)
        {
            rc = fsFileWrite(&g_logFile, g_logFileOffset, tmp_str + tmp_len, LOG_BUF_SIZE, FsWriteOption_Flush);
            if (R_FAILED(rc)) goto end;

            g_logFileOffset += LOG_BUF_SIZE;
            tmp_len += LOG_BUF_SIZE;
            log_str_len -= LOG_BUF_SIZE;
        }

        /* Copy any remaining data from the formatted string into the log buffer. */
        if (log_str_len)
        {
            strcpy(g_logBuffer, tmp_str + tmp_len);
            g_logBufferLength = log_str_len;
        }
    }

#if LOG_FORCE_FLUSH == 1
    /* Flush log buffer. */
    _nxtcLogFlushLogFile();
#endif

end:
    if (tmp_str) free(tmp_str);
}

static void _nxtcLogFlushLogFile(void)
{
    if (!serviceIsActive(&(g_logFile.s)) || !g_logBuffer || !g_logBufferLength) return;

    /* Write log buffer contents and flush the written data right away. */
    Result rc = fsFileWrite(&g_logFile, g_logFileOffset, g_logBuffer, g_logBufferLength, FsWriteOption_Flush);
    if (R_SUCCEEDED(rc))
    {
        /* Update global variables. */
        g_logFileOffset += (s64)g_logBufferLength;
        *g_logBuffer = '\0';
        g_logBufferLength = 0;
    }
}

static bool nxtcLogAllocateLogBuffer(void)
{
    if (g_logBuffer) return true;
    g_logBuffer = nxtcUtilsAlignedAlloc(LOG_BUF_SIZE, LOG_BUF_SIZE);
    return (g_logBuffer != NULL);
}

static bool nxtcLogOpenLogFile(void)
{
    if (serviceIsActive(&(g_logFile.s))) return true;

    Result rc = 0;

    /* Get SD card FsFileSystem object. */
    g_sdCardFileSystem = fsdevGetDeviceFileSystem(DEVOPTAB_SDMC_DEVICE);
    if (!g_sdCardFileSystem) return false;

    /* Create file. This will fail if the logfile exists, so we don't check its return value. */
    fsFsCreateFile(g_sdCardFileSystem, LOG_FILE_NAME, 0, 0);

    /* Open file. */
    rc = fsFsOpenFile(g_sdCardFileSystem, LOG_FILE_NAME, FsOpenMode_Write | FsOpenMode_Append, &g_logFile);
    if (R_SUCCEEDED(rc))
    {
        /* Get file size. */
        rc = fsFileGetSize(&g_logFile, &g_logFileOffset);
        if (R_SUCCEEDED(rc))
        {
            size_t len = 0;

            if (!g_logFileOffset)
            {
                /* Write UTF-8 BOM if the logfile is empty. */
                len = strlen(UTF8_BOM);
                rc = fsFileWrite(&g_logFile, g_logFileOffset, UTF8_BOM, len, FsWriteOption_Flush);
            } else {
                /* Write session separator if the logfile isn't empty. */
                len = strlen(g_logSessionSeparator);
                rc = fsFileWrite(&g_logFile, g_logFileOffset, g_logSessionSeparator, len, FsWriteOption_Flush);
            }

            if (R_SUCCEEDED(rc)) g_logFileOffset += (s64)len;
        }
    }

    /* Close file if we successfully opened it, but an error occurred afterwards. */
    if (R_FAILED(rc) && serviceIsActive(&(g_logFile.s)))
    {
        fsFileClose(&g_logFile);
        memset(&g_logFile, 0, sizeof(FsFile));
    }

    return R_SUCCEEDED(rc);
}

static void nxtcLogGenerateHexStringFromData(char *dst, size_t dst_size, const void *src, size_t src_size)
{
    if (!src || !src_size || !dst || dst_size < ((src_size * 2) + 1)) return;

    size_t i, j;
    const u8 *src_u8 = (const u8*)src;

    for(i = 0, j = 0; i < src_size; i++)
    {
        char h_nib = ((src_u8[i] >> 4) & 0xF);
        char l_nib = (src_u8[i] & 0xF);

        dst[j++] = (h_nib + (h_nib < 0xA ? 0x30 : 0x37));
        dst[j++] = (l_nib + (l_nib < 0xA ? 0x30 : 0x37));
    }

    dst[j] = '\0';
}

#endif  /* DEBUG */
