/*
 * nxtc.c
 *
 * Copyright (c) 2025, DarkMatterCore <pabloacurielz@gmail.com>.
 *
 * This file is part of libnxtc (https://github.com/DarkMatterCore/libnxtc).
 */

#include "nxtc_utils.h"

#define TITLE_CACHE_MAGIC       0x4E585443  /* "NXTC". */
#define TITLE_CACHE_VERSION     1           /* 保持版本1以避免兼容性问题 / Keep version 1 to avoid compatibility issues */
#define TITLE_CACHE_ALIGNMENT   0x10

#define TITLE_CACHE_FILE_NAME   "nxtc_version.bin"
#define TITLE_CACHE_PATH        DEVOPTAB_SDMC_DEVICE HBMENU_BASE_PATH TITLE_CACHE_FILE_NAME

#define NACP_MAX_ICON_SIZE      0x20000     /* 128 KiB. */

/* Type definitions. */

typedef struct {
    u32 magic;          ///< TITLE_CACHE_MAGIC.
    u8 version;         ///< Must be set to TITLE_CACHE_VERSION.
    u8 language;        ///< SetLanguage.
    u8 reserved_1[0x2];
    u32 entry_count;
    u8 reserved_2[0x4];
} NxTitleCacheFileHeader;

LIB_ASSERT(NxTitleCacheFileHeader, 0x10);

typedef struct {
    u64 title_id;
    u16 name_len;
    u16 publisher_len;
    u16 version_len;
    u16 reserved;       ///< 保留字段用于对齐 / Reserved field for alignment.
    u32 version_info;   ///< 数值版本信息 / Numeric version info.
    u32 icon_size;      ///< JPEG icon size. Must not exceed NACP_MAX_ICON_SIZE.
    u32 blob_offset;    ///< Relative to the start of the blob area. Must be aligned to TITLE_CACHE_ALIGNMENT.
    u32 blob_size;
    u32 blob_crc;
    u32 crc;            ///< Calculated over this entire struct with this field set to zero.
} NxTitleCacheFileEntry;

LIB_ASSERT(NxTitleCacheFileEntry, 0x28);  // 更新为实际大小：8+2+2+2+2+4+4+4+4+4+4=40字节=0x28

/* Function prototypes. */

static void nxtcGetSystemLanguage(void);
static const char *nxtcGetPlaceholderString(void);

static void nxtcLoadFile(void);

static NxTitleCacheFileHeader *nxtcLoadFileHeader(u8 *cache_file_data, size_t cache_file_size, size_t *out_cur_offset);
static NxTitleCacheFileEntry *nxtcLoadFileEntries(u8 *cache_file_data, size_t cache_file_size, const NxTitleCacheFileHeader *cache_file_header, size_t *out_cur_offset);
static bool nxtcDeserializeDataBlobs(u8 *cache_file_data, const NxTitleCacheFileHeader *cache_file_header, const NxTitleCacheFileEntry *cache_file_entries, size_t *out_cur_offset);

static void nxtcSaveFile(void);

static bool nxtcPopulateFileHeader(u8 *cache_file_data, size_t *out_cur_offset);
static NxTitleCacheFileEntry *nxtcSerializeDataBlobs(u8 **cache_file_data, size_t *out_cur_offset);

static NxTitleCacheApplicationMetadata *nxtcGenerateCacheEntryFromFileEntry(u8 *cache_file_data, const NxTitleCacheFileEntry *cache_file_entry, size_t *out_cur_offset);
static NxTitleCacheApplicationMetadata *nxtcGenerateCacheEntryFromUserData(u64 title_id, const NacpLanguageEntry *lang_entry, const NacpStruct *nacp, size_t icon_size, const void *icon_data, u32 version_info);
static bool nxtcUpdateCacheEntryWithUserData(NxTitleCacheApplicationMetadata *cache_entry, const NacpLanguageEntry *lang_entry, const NacpStruct *nacp, size_t icon_size, const void *icon_data, u32 version_info);

static void nxtcFillCacheEntryWithUserData(NxTitleCacheApplicationMetadata *cache_entry, const NacpLanguageEntry *lang_entry, const NacpStruct *nacp, size_t icon_size, const void *icon_data, u32 version_info);

static bool nxtcReallocateTitleCache(u32 extra_entry_count, bool free_entries);
static void nxtcFreeTitleCache(bool flush);

static bool nxtcAppendDataBlobToFileCacheBuffer(u8 **cache_file_data, const NxTitleCacheApplicationMetadata *cache_entry, NxTitleCacheFileEntry *cache_file_entry, u32 *out_blob_offset, size_t *out_cur_offset);

static const NacpLanguageEntry *nxtcGetNacpLanguageEntry(const NacpStruct *nacp);

NX_INLINE u32 nxtcCalculateDataBlobSize(u16 name_len, u16 publisher_len, u16 version_len, u32 icon_size);

static NxTitleCacheApplicationMetadata *_nxtcGetApplicationMetadataEntryById(u64 title_id);

static int nxtcEntrySortFunction(const void *a, const void *b);

/* Global variables. */

static Mutex g_nxtcMutex = 0;
static u32 g_nxtcRefCount = 0;

static const SetLanguage g_defaultSystemLanguage = SetLanguage_ENUS;    // Default to American English.
static SetLanguage g_systemLanguage = g_defaultSystemLanguage;
static const char *g_curPlaceholderString = NULL;

static NxTitleCacheApplicationMetadata **g_titleCache = NULL;
static u32 g_titleCacheCount = 0;

static bool g_cacheFlushRequired = false;

/// Provides language-specific placeholders used when a title name/publisher isn't available.
static const char *g_placeholderStrings[SetLanguage_Total] = {
    [SetLanguage_JA]     = "[未知]",            ///< "Michi".
    [SetLanguage_ENUS]   = "[UNKNOWN]",
    [SetLanguage_FR]     = "[INCONNU]",
    [SetLanguage_DE]     = "[UNBEKANNT]",
    [SetLanguage_IT]     = "[SCONOSCIUTO]",
    [SetLanguage_ES]     = "[DESCONOCIDO]",
    [SetLanguage_ZHCN]   = "[未知]",            ///< "Wèizhī".
    [SetLanguage_KO]     = "[알려지지 않은]",   ///< "Allyeojiji anh-eun".
    [SetLanguage_NL]     = "[ONBEKEND]",
    [SetLanguage_PT]     = "[DESCONHECIDO]",
    [SetLanguage_RU]     = "[неизвестный]",     ///< "Neizvestnyy".
    [SetLanguage_ZHTW]   = "[未知]",            ///< "Wèizhī".
    [SetLanguage_ENGB]   = "[UNKNOWN]",
    [SetLanguage_FRCA]   = "[INCONNU]",
    [SetLanguage_ES419]  = "[DESCONOCIDO]",
    [SetLanguage_ZHHANS] = "[未知]",            ///< "Wèizhī".
    [SetLanguage_ZHHANT] = "[未知]",            ///< "Wèizhī".
    [SetLanguage_PTBR]   = "[DESCONHECIDO]"
};

/// Maps SetLanguage values to NacpLanguageEntry indexes.
static const u8 g_nacpLangTable[SetLanguage_Total] = {
    [SetLanguage_JA]     =  2,
    [SetLanguage_ENUS]   =  0,
    [SetLanguage_FR]     =  3,
    [SetLanguage_DE]     =  4,
    [SetLanguage_IT]     =  7,
    [SetLanguage_ES]     =  6,
    [SetLanguage_ZHCN]   = 14,
    [SetLanguage_KO]     = 12,
    [SetLanguage_NL]     =  8,
    [SetLanguage_PT]     = 10,
    [SetLanguage_RU]     = 11,
    [SetLanguage_ZHTW]   = 13,
    [SetLanguage_ENGB]   =  1,
    [SetLanguage_FRCA]   =  9,
    [SetLanguage_ES419]  =  5,
    [SetLanguage_ZHHANS] = 14,
    [SetLanguage_ZHHANT] = 13,
    [SetLanguage_PTBR]   = 15
};

bool nxtcInitialize(void)
{
    bool ret = false;

    SCOPED_LOCK(&g_nxtcMutex)
    {
        /* Check if the interface has already been initialized. */
        if (!g_nxtcRefCount)
        {
            /* Start new log session. */
            NXTC_LOG_MSG(LIB_TITLE " v%u.%u.%u starting. Built on " BUILD_TIMESTAMP ".", LIBNXTC_VERSION_MAJOR, LIBNXTC_VERSION_MINOR, LIBNXTC_VERSION_MICRO);

            /* Get system language. */
            nxtcGetSystemLanguage();

            /* Get placeholder string. */
            g_curPlaceholderString = nxtcGetPlaceholderString();

            /* Load title cache file. */
            nxtcLoadFile();
        }

        /* Update flags. */
        ret = true;
        g_nxtcRefCount++;
    }

    return ret;
}

void nxtcExit(void)
{
    SCOPED_LOCK(&g_nxtcMutex)
    {
        /* Check if the interface has not been initialized. */
        if (!g_nxtcRefCount) break;

        /* Decrement ref counter, if non-zero, then do not close nxtc. */
        g_nxtcRefCount--;
        if (g_nxtcRefCount) break;

        /* Write title cache file and free our title cache. */
        nxtcFreeTitleCache(true);

        /* Restore language variables. */
        g_systemLanguage = g_defaultSystemLanguage;
        g_curPlaceholderString = NULL;

        /* Close logfile. */
        nxtcLogCloseLogFile();
    }
}

bool nxtcCheckIfEntryExists(u64 title_id)
{
    bool ret = false;

    SCOPED_LOCK(&g_nxtcMutex)
    {
        /* Retrieve title cache entry. */
        ret = (_nxtcGetApplicationMetadataEntryById(title_id) != NULL);
    }

    return ret;
}

NxTitleCacheApplicationMetadata *nxtcGetApplicationMetadataEntryById(u64 title_id)
{
    NxTitleCacheApplicationMetadata *out = NULL;

    SCOPED_LOCK(&g_nxtcMutex)
    {
        /* Retrieve title cache entry. */
        NxTitleCacheApplicationMetadata *cache_entry = _nxtcGetApplicationMetadataEntryById(title_id);
        if (!cache_entry)
        {
            NXTC_LOG_MSG("Title cache entry with ID %016lX is unavailable!", title_id);
            break;
        }

        /* Allocate memory for the output element. */
        out = calloc(1, sizeof(NxTitleCacheApplicationMetadata));
        if (!out)
        {
            NXTC_LOG_MSG("Failed to allocate output element for %016lX.", title_id);
            break;
        }

        /* Populate output. */
        out->title_id = title_id;
        out->name = strdup(cache_entry->name);
        out->publisher = strdup(cache_entry->publisher);
        out->version = strdup(cache_entry->version);
        out->version_info = cache_entry->version_info;  // 复制版本信息字段
        out->icon_size = cache_entry->icon_size;
        out->icon_data = malloc(out->icon_size);

        if (out->icon_data) memcpy(out->icon_data, cache_entry->icon_data, out->icon_size);

        if (!out->name || !out->publisher || !out->version || !out->icon_data)
        {
            NXTC_LOG_MSG("Failed to allocate application metadata for %016lX.", title_id);
            nxtcFreeApplicationMetadata(&out);
        }
    }

    return out;
}

bool nxtcAddEntry(u64 title_id, const NacpStruct *nacp, size_t icon_size, const void *icon_data, bool force_add, u32 version_info)
{
    bool ret = false;

    SCOPED_LOCK(&g_nxtcMutex)
    {
        const NacpLanguageEntry *lang_entry = NULL;
        NxTitleCacheApplicationMetadata *cache_entry = NULL;

        if (!title_id || !nacp || !icon_size || icon_size > NACP_MAX_ICON_SIZE || !icon_data)
        {
            NXTC_LOG_MSG("Invalid parameters!");
            break;
        }

        /* Get NACP properties. */
        lang_entry = nxtcGetNacpLanguageEntry(nacp);

        /* Check if the requested title is available within our title cache. */
        cache_entry = _nxtcGetApplicationMetadataEntryById(title_id);
        if (cache_entry)
        {
            /* Return immediately if this operation is not enforced. */
            if (!force_add)
            {
                ret = true;
                break;
            }

            /* Update cache entry with the provided data. */
            if (!nxtcUpdateCacheEntryWithUserData(cache_entry, lang_entry, nacp, icon_size, icon_data, version_info)) break;
        } else {
            /* Generate title cache entry. */
            cache_entry = nxtcGenerateCacheEntryFromUserData(title_id, lang_entry, nacp, icon_size, icon_data, version_info);
            if (!cache_entry) break;

            /* Reallocate title cache entry pointer array. */
            if (!nxtcReallocateTitleCache(1, false))
            {
                nxtcFreeApplicationMetadata(&cache_entry);
                break;
            }

            /* Set title cache entry pointer. */
            g_titleCache[g_titleCacheCount++] = cache_entry;

            /* Sort title cache entries by title ID. */
            if (g_titleCacheCount > 1) qsort(g_titleCache, g_titleCacheCount, sizeof(NxTitleCacheApplicationMetadata*), &nxtcEntrySortFunction);
        }

        /* Update flags. */
        ret = g_cacheFlushRequired = true;
    }

    return ret;
}

bool nxtcGetCacheLanguage(SetLanguage *out_lang)
{
    bool ret = false;

    SCOPED_LOCK(&g_nxtcMutex)
    {
        if (!g_nxtcRefCount || !out_lang) {
            NXTC_LOG_MSG("Invalid parameters!");
            break;
        }

        /* Update values. */
        *out_lang = g_systemLanguage;
        ret = true;
    }

    return ret;
}

void nxtcFlushCacheFile(void)
{
    SCOPED_LOCK(&g_nxtcMutex) nxtcSaveFile();
}

void nxtcWipeCache(void)
{
    SCOPED_LOCK(&g_nxtcMutex)
    {
        /* Check if the interface has already been initialized. */
        if (!g_nxtcRefCount) break;

        /* Free our title cache. */
        nxtcFreeTitleCache(false);

        /* Delete title cache file. */
        remove(TITLE_CACHE_PATH);
        nxtcUtilsCommitSdCardFileSystemChanges();

        /* Update flag. */
        g_cacheFlushRequired = false;
    }
}

/* Loosely based on code from libnx's nacpGetLanguageEntry(). */
static void nxtcGetSystemLanguage(void)
{
    Result rc = 0;
    u64 lang_code = 0;

    /* Initialize set services. */
    rc = setInitialize();
    if (R_SUCCEEDED(rc))
    {
        /* Get system language. */
        rc = setGetSystemLanguage(&lang_code);
        if (R_SUCCEEDED(rc))
        {
            /* Convert the retrieved language code into a SetLanguage value. */
            rc = setMakeLanguage(lang_code, &g_systemLanguage);
            if (R_FAILED(rc)) NXTC_LOG_MSG("setMakeLanguage() failed! (0x%X).", rc);

            /* Use American English for unsupported system languages. */
            if (g_systemLanguage < SetLanguage_JA || (R_SUCCEEDED(rc) && g_systemLanguage >= SetLanguage_Total)) g_systemLanguage = SetLanguage_ENUS;
        }

        /* Close set services. */
        setExit();
    } else {
        NXTC_LOG_MSG("setInitialize() failed! (0x%X).", rc);
    }

    NXTC_LOG_MSG("Retrieved system language: %d.", g_systemLanguage);
}

static const char *nxtcGetPlaceholderString(void)
{
    return (g_systemLanguage < SetLanguage_Total ? g_placeholderStrings[g_systemLanguage] : g_placeholderStrings[SetLanguage_ENUS]);
}

static void nxtcLoadFile(void)
{
    FILE *cache_file = NULL;
    size_t cache_file_size = 0;
    u8 *cache_file_data = NULL;

    NxTitleCacheFileHeader *cache_file_header = NULL;
    NxTitleCacheFileEntry *cache_file_entries = NULL;

    size_t cur_offset = 0;

    bool success = false;

    /* Open title cache file. */
    cache_file = fopen(TITLE_CACHE_PATH, "rb");
    if (!cache_file)
    {
        NXTC_LOG_MSG("Unable to open title cache at \"" TITLE_CACHE_PATH "\" for reading!");
        return;
    }

    /* Get title cache file size and validate it. */
    fseek(cache_file, 0, SEEK_END);
    cache_file_size = ftell(cache_file);
    rewind(cache_file);

    if (!cache_file_size)
    {
        NXTC_LOG_MSG("Title cache file at \"" TITLE_CACHE_PATH "\" is empty!");
        goto end;
    }

    /* Allocate memory for the whole cache file. */
    cache_file_data = malloc(cache_file_size);
    if (!cache_file_data)
    {
        NXTC_LOG_MSG("Failed to allocate 0x%lX byte-long block for the title cache file!", cache_file_size);
        goto end;
    }

    /* Read whole title cache file. */
    if (fread(cache_file_data, 1, cache_file_size, cache_file) != cache_file_size)
    {
        NXTC_LOG_MSG("Failed to read 0x%lX byte-long title cache file! (%d).", cache_file_size, errno);
        goto end;
    }

    /* Load title cache file header. */
    if (!(cache_file_header = nxtcLoadFileHeader(cache_file_data, cache_file_size, &cur_offset))) goto end;

    /* Read title cache file entries. */
    if (!(cache_file_entries = nxtcLoadFileEntries(cache_file_data, cache_file_size, cache_file_header, &cur_offset))) goto end;

    /* Deserialize data blobs. */
    success = nxtcDeserializeDataBlobs(cache_file_data, cache_file_header, cache_file_entries, &cur_offset);

end:
    if (cache_file_data) free(cache_file_data);

    if (cache_file)
    {
        fclose(cache_file);

        if (!success)
        {
            remove(TITLE_CACHE_PATH);
            nxtcUtilsCommitSdCardFileSystemChanges();
        }
    }
}

static NxTitleCacheFileHeader *nxtcLoadFileHeader(u8 *cache_file_data, size_t cache_file_size, size_t *out_cur_offset)
{
    if (!cache_file_data || cache_file_size < sizeof(NxTitleCacheFileHeader) || !out_cur_offset)
    {
        NXTC_LOG_MSG("Invalid parameters!");
        return NULL;
    }

    /* Validate title cache file header. */
    NxTitleCacheFileHeader *cache_file_header = (NxTitleCacheFileHeader*)(cache_file_data + *out_cur_offset);
    if (__builtin_bswap32(cache_file_header->magic) != TITLE_CACHE_MAGIC || cache_file_header->version != TITLE_CACHE_VERSION || \
        cache_file_header->language != (u8)g_systemLanguage || !cache_file_header->entry_count)
    {
        NXTC_LOG_DATA(cache_file_header, sizeof(NxTitleCacheFileHeader), "Invalid title cache file header! Data dump:");
        return NULL;
    }

    /* Update current offset. */
    *out_cur_offset += sizeof(NxTitleCacheFileHeader);

    return cache_file_header;
}

static NxTitleCacheFileEntry *nxtcLoadFileEntries(u8 *cache_file_data, size_t cache_file_size, const NxTitleCacheFileHeader *cache_file_header, size_t *out_cur_offset)
{
    size_t cache_file_entries_size = 0;

    if (!cache_file_data || !cache_file_header || !out_cur_offset || \
        cache_file_size < (*out_cur_offset + (cache_file_entries_size = ((size_t)cache_file_header->entry_count * sizeof(NxTitleCacheFileEntry)))))
    {
        NXTC_LOG_MSG("Invalid parameters!");
        return NULL;
    }

    NxTitleCacheFileEntry *cache_file_entries = NULL;
    u32 cur_blob_offset = 0;
    bool success = false;

    /* Get pointer to title cache file entries. */
    cache_file_entries = (NxTitleCacheFileEntry*)(cache_file_data + *out_cur_offset);

    /* Validate title cache file entries. */
    for(u32 i = 0; i < cache_file_header->entry_count; i++)
    {
        NxTitleCacheFileEntry *cur_cache_file_entry = &(cache_file_entries[i]);
        volatile u32 *crc = (volatile u32*)&(cur_cache_file_entry->crc); // Used to prevent the compiler from optimizing away store/load operations.

        /* Verify entry checksum. */
        u32 stored_crc = *crc;
        *crc = 0;

        u32 calc_crc = crc32Calculate(cur_cache_file_entry, sizeof(NxTitleCacheFileEntry));
        *crc = stored_crc;

        if (calc_crc != stored_crc)
        {
            NXTC_LOG_DATA(cur_cache_file_entry, sizeof(NxTitleCacheFileEntry), "Checksum mismatch on title cache entry #%u! (%08X != %08X). Data dump:", i, calc_crc, stored_crc);
            goto end;
        }

        /* Validate entry fields. */
        u32 calc_blob_size = nxtcCalculateDataBlobSize(cur_cache_file_entry->name_len, cur_cache_file_entry->publisher_len, cur_cache_file_entry->version_len, cur_cache_file_entry->icon_size);

        if (!cur_cache_file_entry->title_id || !cur_cache_file_entry->name_len || !cur_cache_file_entry->publisher_len || !cur_cache_file_entry->version_len || !cur_cache_file_entry->icon_size || cur_cache_file_entry->icon_size > NACP_MAX_ICON_SIZE || \
            !IS_ALIGNED(cur_cache_file_entry->blob_offset, TITLE_CACHE_ALIGNMENT) || cur_cache_file_entry->blob_size != calc_blob_size)
        {
            NXTC_LOG_DATA(cur_cache_file_entry, sizeof(NxTitleCacheFileEntry), "Invalid properties for title cache entry #%u! (blob size 0x%X, next blob offset 0x%X). Data dump:", i, calc_blob_size, cur_blob_offset);
            goto end;
        }

        cur_blob_offset += ALIGN_UP(cur_cache_file_entry->blob_size, TITLE_CACHE_ALIGNMENT);
    }

    /* Validate full title cache file size. */
    size_t full_cache_size = (sizeof(NxTitleCacheFileHeader) + cache_file_entries_size + cur_blob_offset);
    if (cache_file_size < full_cache_size)
    {
        NXTC_LOG_MSG("Title cache file size is too small to hold all data blobs! (0x%lX < 0x%lX).", cache_file_size, full_cache_size);
        goto end;
    }

    /* Update current offset. */
    *out_cur_offset += cache_file_entries_size;

    /* Update flag. */
    success = true;

end:
    return (success ? cache_file_entries : NULL);
}

static bool nxtcDeserializeDataBlobs(u8 *cache_file_data, const NxTitleCacheFileHeader *cache_file_header, const NxTitleCacheFileEntry *cache_file_entries, size_t *out_cur_offset)
{
    if (!cache_file_data || !cache_file_header || !cache_file_entries || !out_cur_offset)
    {
        NXTC_LOG_MSG("Invalid parameters!");
        return false;
    }

    u32 entry_count = cache_file_header->entry_count, extra_entry_count = 0;
    bool success = false, free_entries = false;

    /* Reallocate title cache entry pointer array. */
    if (!nxtcReallocateTitleCache(entry_count, false)) goto end;

    free_entries = true;

    /* Fill new title cache entries. */
    for(u32 i = 0; i < entry_count; i++)
    {
        const NxTitleCacheFileEntry *cur_cache_file_entry = &(cache_file_entries[i]);
        NxTitleCacheApplicationMetadata *cache_entry = NULL;

        /* Check if we have already added a cache entry for the current title ID. */
        cache_entry = _nxtcGetApplicationMetadataEntryById(cur_cache_file_entry->title_id);
        if (cache_entry)
        {
            NXTC_LOG_MSG("Entry already available for %016lX. Skipping cache file entry.", cur_cache_file_entry->title_id);
            continue;
        }

        /* Generate title cache entry. */
        cache_entry = nxtcGenerateCacheEntryFromFileEntry(cache_file_data, cur_cache_file_entry, out_cur_offset);
        if (!cache_entry)
        {
            NXTC_LOG_MSG("Failed to generate title cache entry for %016lX!", cur_cache_file_entry->title_id);
            continue;
        }

        /* Set title cache entry pointer. */
        g_titleCache[g_titleCacheCount + extra_entry_count] = cache_entry;

        /* Increase extra title cache entry counter. */
        extra_entry_count++;
    }

    /* Check retrieved title cache entry count. */
    if (!extra_entry_count)
    {
        NXTC_LOG_MSG("Unable to generate title cache entries! (%u element[s]).", entry_count);
        goto end;
    }

    /* Update title cache entry count. */
    g_titleCacheCount += extra_entry_count;

    /* Free extra allocated pointers if we didn't use them. */
    if (extra_entry_count < entry_count) nxtcReallocateTitleCache(0, false);

    /* Sort title cache entries by title ID. */
    if (g_titleCacheCount > 1) qsort(g_titleCache, g_titleCacheCount, sizeof(NxTitleCacheApplicationMetadata*), &nxtcEntrySortFunction);

    /* Update flag. */
    success = true;

end:
    /* Free previously allocated title cache entry pointers. Ignore return value. */
    if (!success && free_entries) nxtcReallocateTitleCache(extra_entry_count, true);

    return success;
}

static void nxtcSaveFile(void)
{
    u8 *cache_file_data = NULL;

    size_t full_cache_size = (sizeof(NxTitleCacheFileHeader) + ((size_t)g_titleCacheCount * sizeof(NxTitleCacheFileEntry)));
    size_t cur_offset = 0;

    NxTitleCacheFileEntry *cache_file_entries = NULL;

    FILE *cache_file = NULL;

    bool success = false;

    if (!g_nxtcRefCount || !g_titleCache || !g_titleCacheCount)
    {
        NXTC_LOG_MSG("Invalid parameters!");
        return;
    }

    /* Return immediately if there are no pending changes to the title cache file. */
    if (!g_cacheFlushRequired)
    {
        NXTC_LOG_MSG("No changes required.");
        return;
    }

    /* Allocate memory for the title cache file (minus the data blobs). */
    cache_file_data = calloc(1, full_cache_size);
    if (!cache_file_data)
    {
        NXTC_LOG_MSG("Failed to allocate 0x%lX byte-long block for the title cache file data!", full_cache_size);
        goto end;
    }

    /* Populate title cache file header. */
    if (!nxtcPopulateFileHeader(cache_file_data, &cur_offset)) goto end;

    /* Serialize data blobs. */
    if (!(cache_file_entries = nxtcSerializeDataBlobs(&cache_file_data, &cur_offset))) goto end;

    /* Sanity check: make sure the calculated total file size is equal to the current offset. */
    for(u32 i = 0; i < g_titleCacheCount; i++) full_cache_size += ALIGN_UP(cache_file_entries[i].blob_size, TITLE_CACHE_ALIGNMENT);

    if (full_cache_size != cur_offset)
    {
        NXTC_LOG_MSG("Title cache file size mismatch! (0x%lX != 0x%lX).", full_cache_size, cur_offset);
        goto end;
    }

    /* Open title cache file. */
    cache_file = fopen(TITLE_CACHE_PATH, "wb");
    if (!cache_file)
    {
        NXTC_LOG_MSG("Unable to open title cache at \"" TITLE_CACHE_PATH "\" for writing!");
        goto end;
    }

    /* Write full title cache file. */
    if (fwrite(cache_file_data, 1, full_cache_size, cache_file) != full_cache_size)
    {
        NXTC_LOG_MSG("Failed to write 0x%lX byte-long title cache file! (%d).", full_cache_size, errno);
        goto end;
    }

    /* Update flags. */
    g_cacheFlushRequired = false;
    success = true;

end:
    if (cache_file_data) free(cache_file_data);

    if (cache_file)
    {
        fclose(cache_file);
        if (!success) remove(TITLE_CACHE_PATH);
        nxtcUtilsCommitSdCardFileSystemChanges();
    }
}

static bool nxtcPopulateFileHeader(u8 *cache_file_data, size_t *out_cur_offset)
{
    if (!g_titleCacheCount || !cache_file_data || !out_cur_offset)
    {
        NXTC_LOG_MSG("Invalid parameters!");
        return false;
    }

    /* Populate title cache file header. */
    NxTitleCacheFileHeader *cache_file_header = (NxTitleCacheFileHeader*)(cache_file_data + *out_cur_offset);

    memset(cache_file_header, 0, sizeof(NxTitleCacheFileHeader));

    cache_file_header->magic = __builtin_bswap32(TITLE_CACHE_MAGIC);
    cache_file_header->version = TITLE_CACHE_VERSION;
    cache_file_header->language = (u8)g_systemLanguage;
    cache_file_header->entry_count = g_titleCacheCount;

    /* Update current offset. */
    *out_cur_offset += sizeof(NxTitleCacheFileHeader);

    return true;
}

static NxTitleCacheFileEntry *nxtcSerializeDataBlobs(u8 **cache_file_data, size_t *out_cur_offset)
{
    if (!g_titleCacheCount || !cache_file_data || !*cache_file_data || !out_cur_offset)
    {
        NXTC_LOG_MSG("Invalid parameters!");
        return NULL;
    }

    u32 cache_file_entries_offset = *out_cur_offset, cur_blob_offset = 0;
    NxTitleCacheFileEntry *cache_file_entries = (NxTitleCacheFileEntry*)(*cache_file_data + cache_file_entries_offset);
    bool success = false;

    /* Update current offset. */
    *out_cur_offset += ((size_t)g_titleCacheCount * sizeof(NxTitleCacheFileEntry));

    /* Populate title cache file entries and generate data blobs. */
    for(u32 i = 0; i < g_titleCacheCount; i++)
    {
        NxTitleCacheApplicationMetadata *cur_cache_entry = g_titleCache[i];
        NxTitleCacheFileEntry *cur_cache_file_entry = &(cache_file_entries[i]);

        /* Append data blob for the current title cache file entry to our buffer. */
        /* Fair warning: this will reallocate our title cache file buffer. */
        if (!(success = nxtcAppendDataBlobToFileCacheBuffer(cache_file_data, cur_cache_entry, cur_cache_file_entry, &cur_blob_offset, out_cur_offset))) break;

        /* Update pointer to title cache file entries. */
        cache_file_entries = (NxTitleCacheFileEntry*)(*cache_file_data + cache_file_entries_offset);

#ifdef DEBUG
        cur_cache_file_entry = &(cache_file_entries[i]);
        NXTC_LOG_DATA(cur_cache_file_entry, sizeof(NxTitleCacheFileEntry), "Title cache file entry #%u (%p):", i, cur_cache_file_entry);
#endif
    }

    return (success ? cache_file_entries : NULL);
}

static NxTitleCacheApplicationMetadata *nxtcGenerateCacheEntryFromFileEntry(u8 *cache_file_data, const NxTitleCacheFileEntry *cache_file_entry, size_t *out_cur_offset)
{
    if (!cache_file_data || !cache_file_entry || !out_cur_offset)
    {
        NXTC_LOG_MSG("Invalid parameters!");
        return NULL;
    }

    u8 *data_blob = (cache_file_data + *out_cur_offset);
    u32 data_blob_crc = 0;

    NxTitleCacheApplicationMetadata *out = NULL;
    size_t data_offset = 0;

    bool success = false;

    /* Verify blob checksum. */
    data_blob_crc = crc32Calculate(data_blob, cache_file_entry->blob_size);
    if (data_blob_crc != cache_file_entry->blob_crc)
    {
        NXTC_LOG_MSG("Data blob checksum mismatch! (title %016lX) (%08X != %08X).", cache_file_entry->title_id, data_blob_crc, cache_file_entry->blob_crc);
        goto end;
    }

    /* Allocate memory for the output title cache entry. */
    out = malloc(sizeof(NxTitleCacheApplicationMetadata));
    if (!out)
    {
        NXTC_LOG_MSG("Failed to allocate memory for the output title cache entry! (title %016lX).", cache_file_entry->title_id);
        goto end;
    }

    /* Populate title cache entry fields. */
    out->title_id = cache_file_entry->title_id;

    out->name = strndup((char*)data_blob + data_offset, cache_file_entry->name_len);
    data_offset += ALIGN_UP(cache_file_entry->name_len, TITLE_CACHE_ALIGNMENT);

    out->publisher = strndup((char*)data_blob + data_offset, cache_file_entry->publisher_len);
    data_offset += ALIGN_UP(cache_file_entry->publisher_len, TITLE_CACHE_ALIGNMENT);

    out->version = strndup((char*)data_blob + data_offset, cache_file_entry->version_len);
    data_offset += ALIGN_UP(cache_file_entry->version_len, TITLE_CACHE_ALIGNMENT);

    /* 设置数值版本信息 / Set numeric version info */
    out->version_info = cache_file_entry->version_info;

    out->icon_size = cache_file_entry->icon_size;

    out->icon_data = malloc(out->icon_size);
    if (out->icon_data) memcpy(out->icon_data, data_blob + data_offset, out->icon_size);

    if (!out->name || !out->publisher || !out->version || !out->icon_data)
    {
        NXTC_LOG_MSG("Failed to populate output title cache entry! (title %016lX).", cache_file_entry->title_id);
        goto end;
    }

    /* Update current offset. */
    /* Make sure to skip over data blob padding if the data blob size is unaligned. */
    *out_cur_offset += ALIGN_UP(cache_file_entry->blob_size, TITLE_CACHE_ALIGNMENT);

    /* Update flag. */
    success = true;

end:
    if (!success && out) nxtcFreeApplicationMetadata(&out);

    return out;
}

static NxTitleCacheApplicationMetadata *nxtcGenerateCacheEntryFromUserData(u64 title_id, const NacpLanguageEntry *lang_entry, const NacpStruct *nacp, size_t icon_size, const void *icon_data, u32 version_info)
{
    NxTitleCacheApplicationMetadata *out = NULL;
    bool success = false;

    /* Allocate memory for the output title cache entry. */
    out = malloc(sizeof(NxTitleCacheApplicationMetadata));
    if (!out)
    {
        NXTC_LOG_MSG("Failed to allocate memory for the output title cache entry! (title %016lX).", title_id);
        goto end;
    }

    /* Fill title cache entry. */
    out->title_id = title_id;
    nxtcFillCacheEntryWithUserData(out, lang_entry, nacp, icon_size, icon_data, version_info);

    if (!out->name || !out->publisher || !out->version || !out->icon_data)
    {
        NXTC_LOG_MSG("Failed to populate output title cache entry for %016lX!", title_id);
        goto end;
    }

    /* Update flag. */
    success = true;

end:
    if (!success && out) nxtcFreeApplicationMetadata(&out);

    return out;
}

static bool nxtcUpdateCacheEntryWithUserData(NxTitleCacheApplicationMetadata *cache_entry, const NacpLanguageEntry *lang_entry, const NacpStruct *nacp, size_t icon_size, const void *icon_data, u32 version_info)
{
    NxTitleCacheApplicationMetadata bkp_cache_entry = {0};

    /* Backup current title cache entry data in case an error occurs. */
    memcpy(&bkp_cache_entry, cache_entry, sizeof(NxTitleCacheApplicationMetadata));

    /* Fill title cache entry. */
    nxtcFillCacheEntryWithUserData(cache_entry, lang_entry, nacp, icon_size, icon_data, version_info);

    if (!cache_entry->name || !cache_entry->publisher || !cache_entry->version || !cache_entry->icon_data)
    {
        NXTC_LOG_MSG("Failed to populate existent title cache entry for %016lX!", cache_entry->title_id);

        /* Free allocated buffers. */
        if (cache_entry->name) free(cache_entry->name);
        if (cache_entry->publisher) free(cache_entry->publisher);
        if (cache_entry->version) free(cache_entry->version);
        if (cache_entry->icon_data) free(cache_entry->icon_data);

        /* Restore original title cache entry data. */
        memcpy(cache_entry, &bkp_cache_entry, sizeof(NxTitleCacheApplicationMetadata));

        return false;
    }

    /* Free previous title cache entry data. */
    free(bkp_cache_entry.name);
    free(bkp_cache_entry.publisher);
    free(bkp_cache_entry.version);
    free(bkp_cache_entry.icon_data);

    return true;
}

static void nxtcFillCacheEntryWithUserData(NxTitleCacheApplicationMetadata *cache_entry, const NacpLanguageEntry *lang_entry, const NacpStruct *nacp, size_t icon_size, const void *icon_data, u32 version_info)
{
    /* Duplicate title name and publisher strings. */
    if (lang_entry)
    {
        if (lang_entry->name[0])
        {
            cache_entry->name = strndup(lang_entry->name, sizeof(lang_entry->name));
            nxtcUtilsTrimString(cache_entry->name);
        } else {
            cache_entry->name = strdup(g_curPlaceholderString);
        }

        if (lang_entry->author[0])
        {
            cache_entry->publisher = strndup(lang_entry->author, sizeof(lang_entry->author));
            nxtcUtilsTrimString(cache_entry->publisher);
        } else {
            cache_entry->publisher = strdup(g_curPlaceholderString);
        }
        
        /* 从NACP的display_version字段获取版本信息 */
        if (nacp && nacp->display_version[0])
        {
            cache_entry->version = strndup(nacp->display_version, sizeof(nacp->display_version));
            nxtcUtilsTrimString(cache_entry->version);
        } else {
            cache_entry->version = strdup(g_curPlaceholderString);
        }
    } else {
        cache_entry->name = strdup(g_curPlaceholderString);
        cache_entry->publisher = strdup(g_curPlaceholderString);
        cache_entry->version = strdup(g_curPlaceholderString);
    }

    /* 设置数值版本信息 / Set numeric version info */
    cache_entry->version_info = version_info;  // 使用传入的版本信息 / Use the provided version info

    /* Duplicate icon data. */
    cache_entry->icon_size = (u32)icon_size;
    cache_entry->icon_data = malloc(cache_entry->icon_size);

    if (cache_entry->icon_data) memcpy(cache_entry->icon_data, icon_data, cache_entry->icon_size);
}

static bool nxtcReallocateTitleCache(u32 extra_entry_count, bool free_entries)
{
    NxTitleCacheApplicationMetadata **tmp_title_cache = NULL;
    u32 realloc_entry_count = (!free_entries ? (g_titleCacheCount + extra_entry_count) : g_titleCacheCount);
    bool success = false;

    if (free_entries)
    {
        if (!g_titleCache)
        {
            NXTC_LOG_MSG("Invalid parameters!");
            goto end;
        }

        /* Free previously allocated title cache entries. */
        for(u32 i = 0; i <= extra_entry_count; i++) nxtcFreeApplicationMetadata(&(g_titleCache[g_titleCacheCount + i]));
    }

    if (realloc_entry_count)
    {
        /* Reallocate title cache entry pointer array. */
        tmp_title_cache = realloc(g_titleCache, realloc_entry_count * sizeof(NxTitleCacheApplicationMetadata*));
        if (tmp_title_cache)
        {
            /* Update title cache entry pointer. */
            g_titleCache = tmp_title_cache;
            tmp_title_cache = NULL;

            /* Clear new title cache entry pointer array area (if needed). */
            if (realloc_entry_count > g_titleCacheCount) memset(g_titleCache + g_titleCacheCount, 0, extra_entry_count * sizeof(NxTitleCacheApplicationMetadata*));
        } else {
            NXTC_LOG_MSG("Failed to reallocate title cache entry pointer array! (%u element[s]).", realloc_entry_count);
            goto end;
        }
    } else
    if (g_titleCache)
    {
        /* Free title cache entry pointer array. */
        free(g_titleCache);
        g_titleCache = NULL;
    }

    /* Update flag. */
    success = true;

end:
    return success;
}

static void nxtcFreeTitleCache(bool flush)
{
    if (g_titleCache)
    {
        /* Write title cache file. */
        /* This will return immediately if there's no pending changes. */
        if (flush) nxtcSaveFile();

        /* Free title cache entries. */
        for(u32 i = 0; i < g_titleCacheCount; i++) nxtcFreeApplicationMetadata(&(g_titleCache[i]));

        /* Free title cache pointer array. */
        free(g_titleCache);
        g_titleCache = NULL;
    }

    /* Reset title cache entry count. */
    g_titleCacheCount = 0;
}

static bool nxtcAppendDataBlobToFileCacheBuffer(u8 **cache_file_data, const NxTitleCacheApplicationMetadata *cache_entry, NxTitleCacheFileEntry *cache_file_entry, u32 *out_blob_offset, size_t *out_cur_offset)
{
    if (!cache_file_data || !*cache_file_data || !cache_entry || !cache_file_entry || !out_blob_offset || !IS_ALIGNED(*out_blob_offset, TITLE_CACHE_ALIGNMENT) || !out_cur_offset)
    {
        NXTC_LOG_MSG("Invalid parameters!");
        return false;
    }

    u8 *ptr = NULL;
    size_t cache_file_entry_offset = (size_t)((u8*)cache_file_entry - *cache_file_data);
    u32 data_offset = 0, aligned_blob_size = 0;

    bool success = false;

    /* Populate current title cache file entry. */
    cache_file_entry->title_id = cache_entry->title_id;
    cache_file_entry->name_len = (u16)strlen(cache_entry->name);
    cache_file_entry->publisher_len = (u16)strlen(cache_entry->publisher);
    cache_file_entry->version_len = (u16)strlen(cache_entry->version);
    cache_file_entry->reserved = 0;  // 保留字段设为0 / Set reserved field to 0
    cache_file_entry->version_info = cache_entry->version_info;  // 序列化数值版本信息 / Serialize numeric version info
    cache_file_entry->icon_size = cache_entry->icon_size;
    cache_file_entry->blob_offset = *out_blob_offset;
    cache_file_entry->blob_size = nxtcCalculateDataBlobSize(cache_file_entry->name_len, cache_file_entry->publisher_len, cache_file_entry->version_len, cache_file_entry->icon_size);

    /* Reallocate title cache file data buffer to append blob data. */
    aligned_blob_size = ALIGN_UP(cache_file_entry->blob_size, TITLE_CACHE_ALIGNMENT);
    ptr = realloc(*cache_file_data, *out_cur_offset + aligned_blob_size);
    if (!ptr)
    {
        NXTC_LOG_MSG("Failed to reallocate title cache file data buffer!");
        goto end;
    }

    *cache_file_data = ptr;

    /* Adjust pointers after buffer reallocation. */
    cache_file_entry = (NxTitleCacheFileEntry*)(ptr + cache_file_entry_offset);
    ptr += *out_cur_offset;

    /* Populate data blob. */
    memset(ptr + data_offset, 0, ALIGN_UP(cache_file_entry->name_len, TITLE_CACHE_ALIGNMENT));
    memcpy(ptr + data_offset, cache_entry->name, cache_file_entry->name_len);
    data_offset += ALIGN_UP(cache_file_entry->name_len, TITLE_CACHE_ALIGNMENT);

    memset(ptr + data_offset, 0, ALIGN_UP(cache_file_entry->publisher_len, TITLE_CACHE_ALIGNMENT));
    memcpy(ptr + data_offset, cache_entry->publisher, cache_file_entry->publisher_len);
    data_offset += ALIGN_UP(cache_file_entry->publisher_len, TITLE_CACHE_ALIGNMENT);

    memset(ptr + data_offset, 0, ALIGN_UP(cache_file_entry->version_len, TITLE_CACHE_ALIGNMENT));
    memcpy(ptr + data_offset, cache_entry->version, cache_file_entry->version_len);
    data_offset += ALIGN_UP(cache_file_entry->version_len, TITLE_CACHE_ALIGNMENT);

    memcpy(ptr + data_offset, cache_entry->icon_data, cache_entry->icon_size);

    /* Update title cache file entry checksums. */
    cache_file_entry->blob_crc = crc32Calculate(ptr, cache_file_entry->blob_size);

    volatile u32 *crc = (volatile u32*)&(cache_file_entry->crc); // Used to prevent the compiler from optimizing away store/load operations.
    *crc = 0;
    *crc = crc32Calculate(cache_file_entry, sizeof(NxTitleCacheFileEntry));

    /* Update current offsets. */
    /* Make sure to add data blob padding (always needed if the data blob size is unaligned). */
    *out_cur_offset += aligned_blob_size;

    if (aligned_blob_size > cache_file_entry->blob_size)
    {
        u32 padding = (aligned_blob_size - cache_file_entry->blob_size);
        memset(ptr + cache_file_entry->blob_size, 0, padding);
    }

    *out_blob_offset += aligned_blob_size;

    /* Update flag. */
    success = true;

end:
    return success;
}

/* Loosely based on code from libnx's nacpGetLanguageEntry(). */
static const NacpLanguageEntry *nxtcGetNacpLanguageEntry(const NacpStruct *nacp)
{
    if (!nacp) return NULL;

    const NacpLanguageEntry *entry = &(nacp->lang[g_nacpLangTable[g_systemLanguage]]);

    if (!entry->name[0] && !entry->author[0])
    {
        for(u8 i = 0; i < 16; i++)
        {
            entry = &(nacp->lang[i]);
            if (entry->name[0] || entry->author[0]) break;
        }
    }

    if (!entry->name[0] && !entry->author[0]) return NULL;

    return entry;
}

NX_INLINE u32 nxtcCalculateDataBlobSize(u16 name_len, u16 publisher_len, u16 version_len, u32 icon_size)
{
    if (!name_len || !publisher_len || !version_len || !icon_size) return 0;
    return (icon_size + ALIGN_UP(name_len, TITLE_CACHE_ALIGNMENT) + ALIGN_UP(publisher_len, TITLE_CACHE_ALIGNMENT) + ALIGN_UP(version_len, TITLE_CACHE_ALIGNMENT));
}

static NxTitleCacheApplicationMetadata *_nxtcGetApplicationMetadataEntryById(u64 title_id)
{
    if (!g_nxtcRefCount || !g_titleCache || !g_titleCacheCount || !title_id) return NULL;

    for(u32 i = 0; i < g_titleCacheCount; i++)
    {
        NxTitleCacheApplicationMetadata *cur_cache_entry = g_titleCache[i];
        if (cur_cache_entry && cur_cache_entry->title_id == title_id) return cur_cache_entry;
    }

    return NULL;
}

static int nxtcEntrySortFunction(const void *a, const void *b)
{
    const NxTitleCacheApplicationMetadata *title_cache_entry_1 = *((const NxTitleCacheApplicationMetadata**)a);
    const NxTitleCacheApplicationMetadata *title_cache_entry_2 = *((const NxTitleCacheApplicationMetadata**)b);

    if (title_cache_entry_1->title_id < title_cache_entry_2->title_id)
    {
        return -1;
    } else
    if (title_cache_entry_1->title_id > title_cache_entry_2->title_id)
    {
        return 1;
    }

    return 0;
}
