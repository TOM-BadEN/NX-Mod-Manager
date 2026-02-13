# libnxtc
Provides a way for libnx-based homebrew applications for the Nintendo Switch to populate a title cache and store it on the console's SD card.

Main features
--------------

* Serialized binary format that's really easy to parse. No need to clutter your SD card's filesystem with lots of NACPs and JPEGs.
* Keeps it simple by only storing what's absolutely needed from a title's control data: its name, its publisher and its JPEG icon.
* Hardware-accelerated integrity verification using CRC32 checksums.
* Keeps track of the current console language, which means that if it's changed by the user, the library will automatically clear the cache to let the homebrew application populate it once more with proper data.
* Automatically uses language-specific placeholders (e.g. `[UNKNOWN]`, `[DESCONOCIDO]`, etc.) as fallback if a title name and/or publisher string is empty.
* Once the title cache file is created on the SD card, it can be parsed and loaded very quickly (up to 4 seconds for ~600 titles).
* The generated title cache file is saved to `sdmc:/switch/nxtc.bin`, which means that it can be shared and used by multiple homebrew applications (as long as they use this library or an equivalent implementation, of course).

Motivation behind the creation of this library
--------------

With the release of Horizon OS 20.0.0, Nintendo \*really\* messed up a critical call that's used by pretty much every single homebrew application capable of listing software titles available on a Nintendo Switch: `nn::ns::detail::IApplicationManagerInterface::GetApplicationControlData`, which is part of what's collectively known as [NS services](https://switchbrew.org/wiki/NS_services#GetApplicationControlData). This call is used to retrieve a copy of an application's [control.nacp](https://switchbrew.org/wiki/NACP) as well as its JPEG icon.

Prior to the release of the aforementioned update, a single call to the aptly named `nsGetApplicationControlData()` wrapper provided by libnx took about ~5 ms if the data was retrieved from the NS system savefile cache, which was usually the case. However, under HOS 20.0.0+, the same call can take up to 500 ms \*per application\*. As of the time of this writing, we suspect they may be carrying out some sort of software-based icon reescalation steps before returning the requested data -- as to why they do it, we just don't know. It makes no sense at all.

As you can see, this can build up pretty quickly for anyone with many software titles on their Nintendo Switch, which is exactly what some [nxdumptool](https://github.com/DarkMatterCore/nxdumptool) users experienced right after I updated the application to support HOS 20.0.0. A single user reported that the application's boot time went up to approximately \*a whole minute\* after updating their console. We quickly realized something was afoot.

After discovering that `nsGetApplicationControlData()` was at fault, but not before evaluating multiple possible solutions, I decided to come up with a way to safely and efficiently store control data into a single cache file on the SD card that could potentially be used by more than just my own application. This was my [motivation](https://www.youtube.com/watch?v=6JRWqIz3Eow).

Licensing
--------------

This library is distributed under the terms of the ISC License. You can find a copy of this license in the [LICENSE.md file](https://github.com/DarkMatterCore/libnxtc/blob/main/LICENSE.md).

How to install
--------------

This section assumes you've already installed devkitA64, libnx and devkitPro pacman. If not, please follow the steps from the [devkitPro wiki](https://devkitpro.org/wiki/Getting_Started).

Basically, just run `make install` from the root directory of the project -- libnxtc will be installed into the `portlibs` directory from devkitPro, and it'll be ready to use by any homebrew application.

How to use
--------------

This section assumes you've already built the library by following the steps from the previous section.

1. Update the `Makefile` from your homebrew application to reference the library.
    * Two different builds can be generated: a release build (`-lnxtc`) and a debug build with logging enabled (`-lnxtcd`).
    * In case you need to report any bugs, please make sure you're using the debug build and provide its logfile.
2. Include the `nxtc.h` header file somewhere in your code.
3. Initialize the title cache interface with `nxtcInitialize()`.
4. Update your code to issue calls to `nxtcGetApplicationMetadataEntryById()` right before the point(s) where you're already retrieving control data for a single application. This will return an application metadata entry from the cache.
    * Please remember to use `nxtcFreeApplicationMetadata()` to free the returned data after you're done using it.
5. If the previous call fails (e.g. title unavailable within the internal cache):
    * You should then and \*only\* then retrieve control data through regular means, either by using `nsGetApplicationControlData()` or by doing it on your own (e.g. through manual Control NCA parsing, which is faster under HOS 20.0.0+).
    * Calculate the icon size and immediately call `nxtcAddEntry()` to populate the internal title cache.
6. If, for some reason, you need to manually flush the title cache file or wipe the internal tile cache at any given point, just call `nxtcFlushCacheFile()` or `nxtcWipeCache()`, respectively.
7. Close the title cache interface with `nxtcExit()` when you're done.

Please check both the header file located at `/include/nxtc.h` and the provided test application in `/example` for additional information.

Thanks to
--------------

* [ITotalJustice](https://github.com/ITotalJustice), for pointing out the issue with `nsGetApplicationControlData()` and providing multiple benchmarks.
* [cucholix](https://github.com/cucholix), for testing the library under HOS 20.0.0+.
* Switchbrew and libnx contributors.
