/*
 * nxtc_log.h
 *
 * Copyright (c) 2020-2025, DarkMatterCore <pabloacurielz@gmail.com>.
 *
 * This file is part of libnxtc (https://github.com/DarkMatterCore/libnxtc).
 */

#pragma once

#ifndef __NXTC_LOG_H__
#define __NXTC_LOG_H__

#ifdef DEBUG

/// Helper macros.
#define NXTC_LOG_MSG(fmt, ...)                      nxtcLogWriteFormattedStringToLogFile(__FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define NXTC_LOG_DATA(data, data_size, fmt, ...)    nxtcLogWriteBinaryDataToLogFile(data, data_size, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

/// Writes the provided string to the logfile.
void nxtcLogWriteStringToLogFile(const char *src);

/// Writes a formatted log string to the logfile.
__attribute__((format(printf, 4, 5))) void nxtcLogWriteFormattedStringToLogFile(const char *file_name, int line, const char *func_name, const char *fmt, ...);

/// Writes a formatted log string + a hex string representation of the provided binary data to the logfile.
__attribute__((format(printf, 6, 7))) void nxtcLogWriteBinaryDataToLogFile(const void *data, size_t data_size, const char *file_name, int line, const char *func_name, const char *fmt, ...);

/// Forces a flush operation on the logfile.
void nxtcLogFlushLogFile(void);

/// Closes the logfile.
void nxtcLogCloseLogFile(void);

#else   /* DEBUG */

#define NXTC_LOG_MSG(fmt, ...)                      do {} while(0)
#define NXTC_LOG_DATA(data, data_size, fmt, ...)    do {} while(0)

#define nxtcLogWriteStringToLogFile(...)            do {} while(0)
#define nxtcLogWriteFormattedStringToLogFile(...)   do {} while(0)
#define nxtcLogWriteBinaryDataToLogFile(...)        do {} while(0)
#define nxtcLogFlushLogFile(...)                    do {} while(0)
#define nxtcLogCloseLogFile(...)                    do {} while(0)

#endif  /* DEBUG */

#endif /* __NXTC_LOG_H__ */
