# Changelog

v0.0.2:
--------------

### API changes:

* Update the `NxTitleCacheApplicationMetadata` struct to use individual dynamic pointers for the title name and publisher strings.
* Update `nxtcAddEntry()` to take in a `const NacpStruct*` argument instead of individual name and publisher string pointers.
    * The library will select the most suitable `NacpLanguageEntry` on its own, based on the configured system language setting.
    * If an empty string is both detected and unavoidable, a language-specific placeholder string (e.g. `[UNKNOWN]`, `[DESCONOCIDO]`, etc.) will be used as fallback.
* Add `nxtcGetCacheLanguage()`, which provides a `SetLanguage` value that matches the configured system language.
* Add `nxtcWipeCache()`, which completely wipes the internal title cache and deletes the title cache file from the SD card. Useful if, for some reason, a complete cache renewal is required.

### Internal changes:

* Update code to reflect the previously mentioned API changes.
* Remove `NxTitleCacheEntry` struct and simply use `NxTitleCacheApplicationMetadata` instead.
* Move shared logic between `nxtcGenerateCacheEntryFromUserData()` and `nxtcUpdateCacheEntryWithUserData()` into its own function: `nxtcFillCacheEntryWithUserData()`.
* Rename `nxtcReallocateCacheEntryArray()` -> `nxtcReallocateTitleCache()`.
* Add `nxtcFreeTitleCache()`, which is used by both `nxtcExit()` and `nxtcWipeCache()`.
* Add `nxtcGetNacpLanguageEntry()`, which is used to select an appropiate `NacpLanguageEntry` element from user-provided `NacpStruct` elements.
* Rename `nxtcGetEntryById()` -> `_nxtcGetApplicationMetadataEntryById()`.
* Add array of language-specific placeholder strings, as well as the `nxtcGetPlaceholderString()` helper function.
* Other minor changes and bugfixes.

### Changes to example code:

* Remove `utilsInitializeApplicationMetadataFromControlData()`.
* Update `utilsGenerateApplicationMetadataEntryFromNsControlData()` to:
    * Calculate the icon size for the retrieved `NsApplicationControlData`.
    * Immediately call `nxtcAddEntry()` using the retrieved control data to populate the title cache.
    * Retrieve a `NxTitleCacheApplicationMetadata` element from the library itself and return its pointer.
* Update `utilsPrintApplicationMetadataInfo()` to reflect the changes made to the `NxTitleCacheApplicationMetadata` struct.
* Update `utilsGetApplicationMetadata()` to remove its call to `nxtcAddEntry()`.

v0.0.1:
--------------

* Initial release.
