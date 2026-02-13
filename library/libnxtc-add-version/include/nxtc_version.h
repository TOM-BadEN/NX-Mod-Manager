/*
 * nxtc.h
 *
 * Copyright (c) 2025, DarkMatterCore <pabloacurielz@gmail.com>.
 *
 * This file is part of libnxtc (https://github.com/DarkMatterCore/libnxtc).
 */

#pragma once

#ifndef __NXTC_H__
#define __NXTC_H__

#include <switch.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Library version.
#define LIBNXTC_VERSION_MAJOR   0
#define LIBNXTC_VERSION_MINOR   0
#define LIBNXTC_VERSION_MICRO   2

/// Provides a way for the library to return application metadata for a specific title.
typedef struct {
    u64 title_id;       ///< Title ID from the application this data belongs to.
    char *name;         ///< NULL-terminated UTF-8 title name string in the system language.
    char *publisher;    ///< NULL-terminated UTF-8 title publisher string in the system language.
    char *version;      ///< NULL-terminated UTF-8 version string in the system language.
    u32 version_info;   ///< 数值版本信息，用于版本比较和排序 / Numeric version info for version comparison and sorting.
    size_t icon_size;   ///< JPEG icon size.
    void *icon_data;    ///< JPEG icon data.
} NxTitleCacheApplicationMetadata;

/// Initializes the title cache interface by loading and parsing the title cache file from the SD card.
/// Returns false if an error occurs.
bool nxtcInitialize(void);

/// Frees the internal title cache, flushes the title cache file and closes the title cache interface.
void nxtcExit(void);

/// Checks if the provided title ID exists within the internal title cache.
/// Returns false if the provided title ID can't be found, if the title cache interface hasn't been initialized or if the internal title cache is empty.
bool nxtcCheckIfEntryExists(u64 title_id);

/// Provides a pointer to a dynamically allocated NxTitleCacheApplicationMetadata element with data from the internal title cache.
/// Returns NULL if the provided title ID doesn't exist within the internal title cache or if an error occurs.
NxTitleCacheApplicationMetadata *nxtcGetApplicationMetadataEntryById(u64 title_id);

/// Updates the internal title cache to add a new entry with information from the provided arguments.
/// A suitable NacpLanguageEntry is automatically selected depending on the configured system language and the available strings within the provided NacpStruct element.
/// If empty strings are detected and unavoidable, language-specific placeholders will be used as fallback strings.
/// The size for the provided icon must never exceed 0x20000 bytes.
/// Returns true if the new entry has been successfully added, or if the title ID already exists within the internal title cache (unless `force_add` is set to true).
/// Returns false if an error occurs.
bool nxtcAddEntry(u64 title_id, const NacpStruct *nacp, size_t icon_size, const void *icon_data, bool force_add, u32 version_info);

/// Populates the provided SetLanguage element with a value that represents the language being used by the cache (which should match the system language at all times).
/// Returns false if an error occurs.
bool nxtcGetCacheLanguage(SetLanguage *out_lang);

/// Flushes the title cache file to the SD card if there's any pending changes for it.
void nxtcFlushCacheFile(void);

/// Completely wipes the internal title cache and deletes the title cache file from the SD card.
/// Useful if you intentionally want to force a complete cache renewal for some reason.
/// Use with caution.
void nxtcWipeCache(void);

/// Frees a NxTitleCacheApplicationMetadata element.
NX_INLINE void nxtcFreeApplicationMetadata(NxTitleCacheApplicationMetadata **app_metadata)
{
    NxTitleCacheApplicationMetadata *ptr = NULL;
    if (!app_metadata || !(ptr = *app_metadata)) return;

    if (ptr->name) free(ptr->name);
    if (ptr->publisher) free(ptr->publisher);
    if (ptr->version) free(ptr->version);
    if (ptr->icon_data) free(ptr->icon_data);

    free(ptr);
    *app_metadata = NULL;
}

#ifdef __cplusplus
}
#endif

#endif /* __NXTC_H__ */