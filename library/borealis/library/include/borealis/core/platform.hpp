/*
    Copyright 2021 natinusala
    Copyright 2023 xfangfang

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#pragma once

#include <borealis/core/audio.hpp>
#include <borealis/core/font.hpp>
#include <borealis/core/i18n.hpp>
#include <borealis/core/ime.hpp>
#include <borealis/core/input.hpp>
#include <borealis/core/theme.hpp>
#include <borealis/core/video.hpp>
#include <string>

namespace brls
{

// Interface to provide everything platform specific required to run borealis: graphics context, inputs, audio...
// The best platform is automatically selected when the application starts, and cannot be changed by the user at the moment
class Platform
{
  public:
    virtual ~Platform() = default;;

    /**
     * Called on startup, right after instanciation, to create and open a window
     * with the given title and size.
     */
    virtual void createWindow(std::string title, uint32_t width, uint32_t height, float windowXPos = NAN, float windowYPos = NAN) = 0;

    /**
     *
     * This function also restores windows from maximization.
     *
     */
    virtual void restoreWindow() {};

    /**
     *
     * Set window size
     *
     */
    virtual void setWindowSize(uint32_t windowWidth, uint32_t windowHeight) {};

    /**
     *
     * Set window size limits
     *
     */
    virtual void setWindowSizeLimits(uint32_t windowMinWidth, uint32_t windowMinHeight, uint32_t windowMaxWidth, uint32_t windowMaxHeight) {};

    /**
     *
     * Set window position
     *
     */
    virtual void setWindowPosition(int windowXPos, int windowYPos) {};

    /**
     *
     * 1.restoreWindow
     * 2.Set window size
     * 3.Set window position
     *
     */
    virtual void setWindowState(uint32_t windowWidth, uint32_t windowHeight, int windowXPos, int windowYPos) {};

    /**
     * Returns the human-readable name of the platform.
     */
    virtual std::string getName() = 0;

    /**
     * Returns true if device has wireless connection.
     */
    virtual bool hasWirelessConnection() { return false; };

    /**
     * Returns wireless quality level from 0 to 3.
     */
    virtual int getWirelessLevel() = 0;

    /**
     * Returns true if device has ethernet connection.
     */
    virtual bool hasEthernetConnection() { return false; };

    /**
     * Returns ip address.
     */
    virtual std::string getIpAddress() = 0;

    /**
     * Returns dns server address.
     */
    virtual std::string getDnsServer() = 0;

    /**
     * Returns if battery level supports.
     */
    virtual bool canShowBatteryLevel() { return true; }

    /**
     * Returns if Wireless level supports.
     */
    virtual bool canShowWirelessLevel() { return true; }

    /**
     * Returns the battery level from 0 to 100.
     */
    virtual int getBatteryLevel() = 0;

    /**
     * Returns the battery level from 0 to 100.
     */
    virtual bool isBatteryCharging() = 0;

    /**
     * Disable screen dimming/saver...
     *
     * @param disable Inhibit or not
     * @param reason If disable is true, pass Inhibited reason to the system.
     * @param app If disable is true, pass app name to the system.
     */
    virtual void disableScreenDimming(bool disable, const std::string& reason = "disable dimming", const std::string& app = "borealis") = 0;

    /**
     * Returns true if screen dimming is disabled
     */
    virtual bool isScreenDimmingDisabled() = 0;

    /**
     * Set the backlight brightness
     *
     * @param brightness 0.0 to 1.0
     */
    virtual void setBacklightBrightness(float brightness) = 0;

    /**
     * Get the backlight brightness
     * @return 0.0 to 1.0
     */
    virtual float getBacklightBrightness() = 0;

    /**
     * Returns true if the platform can set/get the backlight brightness
     */
    virtual bool canSetBacklightBrightness() = 0;

    /**
     * Set the windows always on top
     */
    virtual void setWindowAlwaysOnTop(bool enable) {}

    /**
     * Called at every iteration of the main loop.
     * Must return false if the app should continue running
     * (for example, return false if the X button was pressed on the window).
     */
    virtual bool mainLoopIteration() = 0;

    virtual bool runLoop(const std::function<bool()>& runLoopImpl) { return runLoopImpl(); }

    /**
     * Can be called at anytime to get the current system theme variant.
     *
     * For now, the variant is assumed to stay the same during the whole time
     * the app is running (no variant hot swap).
     *
     * As such, the result should be cached by the platform code.
     */
    virtual ThemeVariant getThemeVariant() = 0;

    /**
     * Overwrite current theme settings
     */
    virtual void setThemeVariant(ThemeVariant theme) = 0;

    /**
     * Can be called at anytime to get the current locale
     *
     * For now, the locale is assumed to stay the same during the whole time
     * the app is running (no locale hot swap)
     *
     * As such, the result should be cached by the platform code.
     * The method should return one of the locale constants
     * defined in the i18n header file.
     */
    virtual std::string getLocale() = 0;

    /**
     * Returns the AudioPlayer for the platform.
     * Cannot return nullptr.
     */
    virtual AudioPlayer* getAudioPlayer() = 0;

    /**
     * Returns the VideoContext for the platform.
     * Cannot return nullptr.
     */
    virtual VideoContext* getVideoContext() = 0;

    /**
     * Returns the InputManager for the platform.
     * Cannot return nullptr.
     */
    virtual InputManager* getInputManager() = 0;

    /**
     * Returns the ImeManager for the platform.
     * Cannot return nullptr.
     */
    virtual ImeManager* getImeManager() = 0;

    /**
     * Returns the FontLoader for the platform.
     * Cannot return nullptr.
     */
    virtual FontLoader* getFontLoader() = 0;

    /**
     * Returns if non applet.
     */
    virtual bool isApplicationMode() = 0;

    /**
     * When the program exits, exit directly to the home menu
     */
    virtual void exitToHomeMode(bool value) = 0;

    /**
     * When the program exits, exit directly to the home menu
     */
    virtual void forceEnableGamePlayRecording() = 0;

    /**
     * Open a link in browser
     * @param url the link which will be opened
     */
    virtual void openBrowser(std::string url) = 0;


    /**
     * Paste text to clipboard
     */
    virtual void pasteToClipboard(const std::string& text) {};

    /**
     * Copy text from clipboard
     */
    virtual std::string pasteFromClipboard() {return "";};

    /**
     * Selects and returns the best platform.
     */
    static Platform* createPlatform();

    /**
     * App locale, empty for default
     */
    static inline std::string APP_LOCALE_DEFAULT = LOCALE_AUTO;
};

} // namespace brls
