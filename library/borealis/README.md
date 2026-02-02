![borealis logo](https://github.com/natinusala/borealis/blob/main/resources/img/borealis_96.png?raw=true)
# borealis

Controller and TV oriented UI library for Android, iOS, PC, PS4, PSV and Nintendo Switch.

- Mimicks the Nintendo Switch system UI, but can also be used to make anything else painlessly
- Hardware acceleration and vector graphics with automatic scaling for TV usage (powered by nanovg)
- Can be ported to new platforms and graphics APIs by providing a nanovg implementation
- Powerful layout engine using flex box as a base for everything (powered by Yoga Layout)
- Automated navigation paths for out-of-the-box controller navigation
- Out of the box touch support
- Define user interfaces using XML and only write code when it matters
- Use and restyle built-in components or make your own from scratch
- Display large amount of data efficiently using recycling lists
- Integrated internationalization and storage systems
- Integrated toolbox (logger, animations, timers, background tasks...)

Check the [daily builds](https://github.com/xfangfang/borealis/actions) for what borealis looks like.

## Build the demo code

Check our wiki for more information:

- [Android](https://github.com/xfangfang/borealis/wiki/Android)
- [iOS](https://github.com/xfangfang/borealis/wiki/iOS)
- [Windows macOS Linux](https://github.com/xfangfang/borealis/wiki/Windows-macOS-Linux)
- [NintendoSwitch](https://github.com/xfangfang/borealis/wiki/NintendoSwitch)
- [PS Vita](https://github.com/xfangfang/borealis/wiki/PS-Vita)
- [PS4](https://github.com/xfangfang/borealis/wiki/PS4)

## Including in your project

1. Your project must be built as C++17 (`-std=c++1z`). You also need to remove `-fno-rtti` and `-fno-exceptions` if you have them
2. Use a submodule (or even better, a [subrepo](https://github.com/ingydotnet/git-subrepo)) to clone this repository in your project
3. Copy the `resources` folder to the root of your project

Or you can start a new project from the [template](https://github.com/xfangfang/borealis_template)


### Made with borealis (Alphabetical order)

| **Project Name** | **Project Description** |
| --- | --- |
| [Moonlight-Switch](https://github.com/XITRIX/Moonlight-Switch) | Moonlight port for Nintendo Switch |
| [SimpleModDownloader](https://github.com/PoloNX/SimpleModDownloader) | A switch homebrew which downloads mods from gamebanana |
| [switchfin](https://github.com/dragonflylee/switchfin) | Third-party native Jellyfin client for PC/PS4/Nintendo Switch |
| [UpcomingSwitchGames](https://github.com/PoloNX/UpcomingSwitchGames) | A switch homebrew for viewing upcoming games |
| [wiliwili](https://github.com/xfangfang/wiliwili) | Third-party bilibili client for PC/PS4/PSV/XBOX/Nintendo Switch |party bilibili client for PC/PS4/PSV/XBOX/Nintendo Switch | [https://github.com/xfangfang/wiliwili](https://github.com/xfangfang/wiliwili) || Third-party bilibili client for PC/PS4/PSV/XBOX/Nintendo Switch | [https://github.com/xfangfang/wiliwili](https://github.com/xfangfang/wiliwili) |