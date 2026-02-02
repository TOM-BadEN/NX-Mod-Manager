/*
    Copyright 2019-2021 natinusala
    Copyright 2019 p-sam
    Copyright 2021 XITRIX

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

#include <borealis/core/activity.hpp>
#include <borealis/core/audio.hpp>
#include <borealis/core/font.hpp>
#include <borealis/core/frame_context.hpp>
#include <borealis/core/logger.hpp>
#include <borealis/core/platform.hpp>
#include <borealis/core/style.hpp>
#include <borealis/core/theme.hpp>
#include <borealis/core/view.hpp>
#include <borealis/core/notification_manager.hpp>
#include <borealis/views/label.hpp>
#include <deque>
#include <vector>

#ifdef __WINRT__
#ifdef main
#undef main
#endif
#define main SDL_main
#endif

namespace brls
{

// Input types for entire app
enum class InputType
{
    GAMEPAD, // Gamepad or keyboard
    TOUCH, // Touch screen
};

class DebugLayer;
class EditTextDialog;

typedef std::function<View*(void)> XMLViewCreator;

class Application
{
  public:
    friend class EditTextDialog;

    static inline uint32_t ORIGINAL_WINDOW_WIDTH  = 1280;
    static inline uint32_t ORIGINAL_WINDOW_HEIGHT = 720;

    /**
     * Inits the borealis application.
     * Returns true if it succeeded, false otherwise.
     */
    static bool init();

    /**
     * Creates the application window with the given title.
     * Must be called after calling init().
     */
    static void createWindow(std::string title);

    /**
     * Application main loop iteration.
     * Must be called in an infinite loop until it returns false.
     */
    static bool mainLoop();

    static Platform* getPlatform();
    static AudioPlayer* getAudioPlayer();

    static NVGcontext* getNVGContext();
    inline static float contentWidth, contentHeight;

    inline static unsigned windowWidth, windowHeight;

    inline static int windowXPos, windowYPos;

    /**
     * Called by the video context when the content window is resized
     */
    static void onWindowResized(int width, int height);

    /**
     * Do not call this function, use it internally.
     * Called when the video context is ready (to setup the initial content scaling).
     */
    static void setWindowSize(int width, int height);

    /**
     * Called by the video context when the content window is Repositioned
     */
    static void onWindowReposition(int xPos, int yPos);

    /**
     * Do not call this function, use it internally.
     * Called when the video context is ready (to setup the initial window position).
     */
    static void setWindowPosition(int xPos, int yPos);

    static std::vector<Activity*> getActivitiesStack();

    /**
     * Pushes a view on this applications's view stack.
     *
     * The view will automatically be resized to take
     * the whole screen.
     *
     * The view will gain focus if applicable.
     *
     * The first activity to be pushed cannot be popped.
     */
    static void pushActivity(Activity* view, TransitionAnimation animation = TransitionAnimation::FADE);

    /**
     * Pops the last pushed activity from the stack
     * and gives focus back where it was before.
     *
     * return false if no actifity to pop.
     */
    static bool popActivity(
        TransitionAnimation animation = TransitionAnimation::FADE, std::function<void(void)> cb = [] {}, bool free = true);

    /**
     * Gives the focus to the given view
     * or clears the focus if given nullptr.
     */
    static void giveFocus(View* view);

    inline static Style getStyle()
    {
        return brls::getStyle();
    }

    static Theme getTheme();
    static ThemeVariant getThemeVariant();

    static ImeManager* getImeManager();

    /**
     * Loads a font from a given file and stores it in the font stash.
     * Returns true if the operation succeeded.
     */
    static bool loadFontFromFile(std::string fontName, std::string filePath);

    /**
     * Loads a font from a given memory buffer and stores it in the font stash.
     * Returns true if the operation succeeded.
     */
    static bool loadFontFromMemory(std::string fontName, void* data, size_t size, bool freeData);

    /**
     * Returns the nanovg handle to the given font name, or FONT_INVALID if
     * no such font is currently loaded.
     */
    static int getFont(std::string fontName);

    static int getDefaultFont();

    static void notify(const std::string& text);

    static void onControllerButtonPressed(enum ControllerButton button, bool repeating);

    static void onKeyboardPressed(BrlsKeyCombination key, bool repeating);

    /**
     * "Crashes" the app (displays a fullscreen CrashFrame)
     */
    static void crash(std::string text);

    static void quit();

    /**
     * Blocks any and all user inputs
     */
    static void blockInputs(bool muteSounds = false);

    /**
     * Unblocks inputs after a call to
     * blockInputs()
     */
    static void unblockInputs();

    static bool isInputBlocks();

    static const ControllerState& getControllerState();

    static void setCommonFooter(std::string footer);
    static std::string* getCommonFooter();

    inline static float windowScale;

    /**
     * Sets whether BUTTON_START will globally be used to close the application.
     */
    static void setGlobalQuit(bool enabled);

    static void setFPSStatus(bool enabled);
    static bool getFPSStatus();
    static size_t getFPS();

    /**
     * Set the FPS limit
     * @param fps 0 to disable limit
     */
    static void setLimitedFPS(size_t fps);

    /**
     * Set the swap interval
     * Must be called after createWindow. On some platforms, it is impossible to set it back
     * to 0 after setting it to a non-zero values, so you can use
     * `VideoContext::swapInterval = 0;` (#include "borealis/core/video.hpp") before creating the window.
     *
     * Test results:
     *
     * ## Platforms that always vsync (interval: 1):
     * iOS18/GLES3/SDL
     * PS4/GLES2/SDL
     *
     * ## Platforms that only support enable and disable vsync (interval: 0,1):
     * Android9/GLES2/SDL
     * macOS13.0+/GL3.2/GLFW (will fixed to 120fps with vsync enabled, https://github.com/glfw/glfw/pull/2277)
     * Windows11/GL3.2/GLFW
     * Linux Wayland/GL3.2/GLFW
     *
     * ## Platforms that not support disable vsync (interval: 1,2,3,4):
     * Windows11/D3D11/GLFW
     * Switch/DEKO3D
     *
     * ## Platforms that support arbitrary values (interval: 0,1,2,3,4):
     * PsVita/GXM
     * PsVita/GLES2/SDL
     * macOS13.0+/GL3.2/SDL
     * Linux X11/GL3.2/GLFW
     * Linux X11/GL3.2/SDL
     * Linux Wayland/GL3.2/SDL
     * Switch/GL4.3/GLFW
     * Switch/GL4.3/SDL
     *
     * @param interval 0 to disable vsync
     */
    static void setSwapInterval(int interval);

    /**
     * If the value is set to true, the program will limit FPS to Application::DeactivatedFPS
     * after Application::DeactivatedTime milliseconds of inactivity.
     *
     * default is false;
     */
    static void setAutomaticDeactivation(bool value);
    static bool getAutomaticDeactivation();
    static bool hasActiveEvent();
    static void setActiveEvent(bool value);
    static void setDeactivatedTime(int millisecond);
    static void setDeactivatedFPS(int value);
    static int getDeactivatedFPS();
    static double getDeactivatedFrameTime();

    static GenericEvent* getGlobalFocusChangeEvent();
    static VoidEvent* getGlobalHintsUpdateEvent();
    static Event<InputType>* getGlobalInputTypeChangeEvent();
    static VoidEvent* getRunLoopEvent();
    static VoidEvent* getExitEvent();
    static VoidEvent* getExitDoneEvent();
    static VoidEvent* getWindowSizeChangedEvent();
    static VoidEvent* getWindowCreationDoneEvent();
    static VoidEvent* getWindowShouldCloseEvent();
    static Event<bool>* getWindowFocusChangedEvent();

    static View* getCurrentFocus();

    static std::string getTitle();

    /**
     * Registers a view to be created from XML. You must give the name of the XML node as well
     * as a function that creates the view.
     *
     * If you need attributes, register them with the given functions in the view
     * class constructor directly. They will be called one by one after the view is instantiated.
     *
     * You should not add any children in the function, it is already taken care of.
     */
    static void registerXMLView(std::string name, XMLViewCreator creator);

    static bool XMLViewsRegisterContains(std::string name);
    static XMLViewCreator getXMLViewCreator(std::string name);

    /**
     * Returns the current system locale.
     */
    static std::string getLocale();

    static void addToFreeQueue(View* view);

    /**
     * Returns the current input type.
     */
    inline static InputType getInputType()
    {
        return inputType;
    }

    inline static void enableDebuggingView(bool enable)
    {
        debuggingViewEnabled = enable;
    }

    inline static bool isDebuggingViewEnabled()
    {
        return debuggingViewEnabled;
    }

    static void setSwapInputKeys(bool swap);

    inline static bool isSwapInputKeys()
    {
        return swapInputKeys;
    }

    inline static void setDrawCoursor(bool draw)
    {
        drawCoursor = draw;
    }

    inline static bool isDrawCursor()
    {
        return drawCoursor;
    }

    inline static bool isHintsLiteMode()
    {
        return hintsLiteMode;
    }

    // Set hints lite mode, which will only show hints for A button and B button
    inline void static setHintsLiteMode(const bool value)
    {
        hintsLiteMode = value;
    }

    static void tryDeinitFirstResponder(View* view);

    static void addToWatchedKeys(const BrlsKeyCombination key);

    static void removeWatchedKeys(const BrlsKeyCombination key);

  private:
    inline static bool inited               = false;
    inline static bool quitRequested        = false;
    inline static bool debuggingViewEnabled = false;
    inline static bool swapInputKeys        = false;
    inline static bool drawCoursor          = false;

    inline static Platform* platform = nullptr;

    inline static std::string title;

    inline static FontStash fontStash;

    inline static std::vector<Activity*> activitiesStack;
    inline static std::vector<View*> focusStack;
    inline static std::deque<View*> deletionPool;

    inline static View* currentFocus = nullptr;
    inline static std::vector<TouchState> currentTouchState;
    inline static MouseState currentMouseState;
    inline static NotificationManager* notificationManager;

    // Return true if input type was changed
    static bool setInputType(InputType type);

    inline static InputType inputType = InputType::GAMEPAD;
    inline static std::vector<BrlsKeyState> watchedKeys;
    inline static std::vector<BrlsKeyState> oldWatchedKeys;
    inline static std::unordered_map<int, int> watchedKeysMap;
    inline static ControllerState controllerState = {};

    inline static void processInput();
    inline static bool internalMainLoop();

    inline static void updateFPS();

    inline static unsigned blockInputsTokens = 0; // any value > 0 means inputs are blocked
    inline static bool muteSounds            = false;

    inline static std::string commonFooter;

    inline static bool globalQuitEnabled                = false;
    inline static ActionIdentifier gloablQuitIdentifier = ACTION_NONE;
    inline static bool globalFPSToggleEnabled           = false;
    inline static size_t globalFPS                      = 60;
    inline static Time limitedFrameTime                 = 0;
    inline static Time frameStartTime                   = 0;
    inline static bool hintsLiteMode                    = false;

    inline static bool deactivatedBehavior = false;
    inline static bool activeEvent         = false;
    inline static Time lastActiveTime      = 0;
    inline static int deactivatedFPS       = 5; // FPS 5
    inline static int deactivatedTime      = 5000000; // 5s

    inline static View* repetitionOldFocus = nullptr;

    inline static GenericEvent globalFocusChangeEvent;
    inline static VoidEvent globalHintsUpdateEvent;
    inline static Event<InputType> globalInputTypeChangeEvent;
    inline static VoidEvent runLoopEvent;
    inline static VoidEvent exitEvent;
    inline static VoidEvent exitDoneEvent;
    inline static VoidEvent windowSizeChangedEvent;
    inline static VoidEvent windowCreationDoneEvent;
    inline static VoidEvent windowShouldCloseEvent;
    inline static Event<bool> windowFocusChangedEvent;

    inline static std::unordered_map<std::string, XMLViewCreator> xmlViewsRegister;

    static void navigate(FocusDirection direction, bool repeating);

    static void frame();
    static void clear();
    static void exit();

    /**
     * Handles actions for the currently focused view and
     * the given button
     * Returns true if at least one action has been fired
     */
    static bool handleAction(ActionType type, int button, bool repeating);

    static void registerBuiltInXMLViews();

    inline static DebugLayer* debugLayer = nullptr;
};

} // namespace brls
