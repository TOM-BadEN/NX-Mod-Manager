/*
    Copyright 2021 natinusala
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

#include <borealis/core/application.hpp>
#include <borealis/core/logger.hpp>
#include <borealis/platforms/desktop/desktop_platform.hpp>
#include <borealis/platforms/glfw/glfw_input.hpp>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <unistd.h>

namespace brls
{

#define GLFW_GAMEPAD_BUTTON_NONE SIZE_MAX
#define GLFW_GAMEPAD_BUTTON_MAX 15
#define GLFW_GAMEPAD_AXIS_MAX 6

// LT and RT do not exist here because they are axes
static const size_t GLFW_BUTTONS_MAPPING[GLFW_GAMEPAD_BUTTON_MAX] = {
    BUTTON_A, // GLFW_GAMEPAD_BUTTON_A
    BUTTON_B, // GLFW_GAMEPAD_BUTTON_B
    BUTTON_X, // GLFW_GAMEPAD_BUTTON_X
    BUTTON_Y, // GLFW_GAMEPAD_BUTTON_Y
    BUTTON_LB, // GLFW_GAMEPAD_BUTTON_LEFT_BUMPER
    BUTTON_RB, // GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER
    BUTTON_BACK, // GLFW_GAMEPAD_BUTTON_BACK
    BUTTON_START, // GLFW_GAMEPAD_BUTTON_START
    BUTTON_GUIDE, // GLFW_GAMEPAD_BUTTON_GUIDE
    BUTTON_LSB, // GLFW_GAMEPAD_BUTTON_LEFT_THUMB
    BUTTON_RSB, // GLFW_GAMEPAD_BUTTON_RIGHT_THUMB
    BUTTON_UP, // GLFW_GAMEPAD_BUTTON_DPAD_UP
    BUTTON_RIGHT, // GLFW_GAMEPAD_BUTTON_DPAD_RIGHT
    BUTTON_DOWN, // GLFW_GAMEPAD_BUTTON_DPAD_DOWN
    BUTTON_LEFT, // GLFW_GAMEPAD_BUTTON_DPAD_LEFT
};

static const size_t GLFW_AXIS_MAPPING[GLFW_GAMEPAD_AXIS_MAX] = {
    LEFT_X,
    LEFT_Y,
    RIGHT_X,
    RIGHT_Y,
    LEFT_Z,
    RIGHT_Z,
};

static short controllersCount = 0;

static void glfwJoystickCallback(int jid, int event)
{
    if (event == GLFW_CONNECTED)
    {
        Logger::info("glfw: joystick {} connected", jid);

        if (glfwJoystickIsGamepad(jid))
        {
            Logger::info("glfw: joystick {} is gamepad: \"{}\"", jid, glfwGetGamepadName(jid));
        }

        controllersCount++;
    }
    else if (event == GLFW_DISCONNECTED)
    {
        Logger::info("glfw: joystick {} disconnected", jid);

        controllersCount--;
    }
    Application::setActiveEvent(true);
}

#define GLFW_STICKY 4
static RawTouchState touchState = { 0, 0, { 0, 0 } };
static int touchStateStatus = GLFW_RELEASE;
static bool touchUpdate = false;

static inline int getTouchState()
{
    if (touchStateStatus == GLFW_STICKY)
    {
        touchStateStatus = GLFW_RELEASE;
        // Try to ignore the touch end event as this can lead to a smaller calculation of scrolling acceleration
        int res = touchUpdate ? GLFW_PRESS : GLFW_RELEASE;
        touchUpdate = false;
        return res;
    }
    else
    {
        touchUpdate = false;
        return touchStateStatus;
    }
}

static void glfwTouchCallback(GLFWwindow* window, int touch, int action, double xpos, double ypos)
{
#if defined(BOREALIS_USE_D3D11)
    float scaleFactor = 1.0f / Application::windowScale;
#else
    double scaleFactor = Application::getPlatform()->getVideoContext()->getScaleFactor() / Application::windowScale;
#endif
    touchState.fingerId   = 0;
    touchState.pressed    = action != GLFW_RELEASE;
    touchState.position.x = (float)(xpos * scaleFactor);
    touchState.position.y = (float)(ypos * scaleFactor);
    touchStateStatus      = touchState.pressed ? GLFW_PRESS : GLFW_STICKY;
    touchUpdate          |= touchState.pressed;
    Application::setActiveEvent(true);
}

void GLFWInputManager::keyboardCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    auto* self = (GLFWInputManager*)Application::getPlatform()->getInputManager();
    KeyState state {};
    state.key            = (BrlsKeyboardScancode)key;
    state.mods           = mods;
    state.pressed        = action != GLFW_RELEASE;
    const char* key_name = glfwGetKeyName(key, scancode);
    if (key_name != NULL)
        Logger::debug("Key: {} / Code: {} / Action: {}", key_name, key, action);
    else
        Logger::debug("Key: NULL / Code: {} / Action: {}", key, action);
    self->getKeyboardKeyStateChanged()->fire(state);
    Application::setActiveEvent(true);
}

void GLFWInputManager::scrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    auto* self = (GLFWInputManager*)Application::getPlatform()->getInputManager();
#if defined(_WIN32) || defined(__linux__)
    self->scrollOffset.x += xoffset * 30;
    self->scrollOffset.y += yoffset * 30;
#else
    self->scrollOffset.x += xoffset * 10;
    self->scrollOffset.y += yoffset * 10;
#endif
    self->getMouseScrollOffsetChanged()->fire(Point(xoffset, yoffset));
    Application::setActiveEvent(true);
}

void GLFWInputManager::cursorCallback(GLFWwindow* window, double x, double y)
{
    auto* self = (GLFWInputManager*)Application::getPlatform()->getInputManager();
    if (self->pointerLocked)
    {
        int width, height;
        glfwGetWindowSize(self->window, &width, &height);

        int hWidth  = width / 2;
        int hHeight = height / 2;

        Point localPointerOffset;

        localPointerOffset.x = (x - hWidth); // / Application::windowScale;
        localPointerOffset.y = (y - hHeight); // / Application::windowScale;

        self->pointerOffsetBuffer.x += (x - hWidth); // / Application::windowScale;
        self->pointerOffsetBuffer.y += (y - hHeight); // / Application::windowScale;

        self->getMouseCusorOffsetChanged()->fire(localPointerOffset);
        glfwSetCursorPos(self->window, hWidth, hHeight);
    }
}

GLFWInputManager::GLFWInputManager(GLFWwindow* window)
    : window(window)
{
    if (access(DesktopPlatform::GAMEPAD_DB.c_str(), F_OK) == -1)
    {
        brls::Logger::warning("Cannot find custom gamepad db, (Searched at: {})",
            DesktopPlatform::GAMEPAD_DB);
    }
    else
    {
        const std::string mappings = loadFileContents(DesktopPlatform::GAMEPAD_DB);
        brls::Logger::info("Load custom gamepad db: {}", DesktopPlatform::GAMEPAD_DB);
        glfwUpdateGamepadMappings(mappings.c_str());
    }

    glfwSetJoystickCallback(glfwJoystickCallback);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetCursorPosCallback(window, cursorCallback);
    glfwSetKeyCallback(window, keyboardCallback);
    if (glfwTouchInputSupported())
    {
        glfwSetInputMode(window, GLFW_TOUCH, GLFW_TRUE);
        glfwSetTouchCallback(window, glfwTouchCallback);
    }

    for (int i = 0; i < GAMEPADS_MAX; i++)
    {
        const char* guid = glfwGetJoystickGUID(i);
        if (guid)
            Logger::info("glfw: joystick {} GUID {}", i, guid);
        if (glfwJoystickIsGamepad(i))
        {
            Logger::info("glfw: joystick {} connected", i);
            Logger::info("glfw: joystick {} is gamepad: \"{}\"", i, glfwGetGamepadName(i));
        }
    }

    Application::getRunLoopEvent()->subscribe([this]()
        {
#if defined(_WIN32) || defined(__linux__)
        // smooth scroll
        if(fabs(scrollOffset.y) < 1) scrollOffset.y = 0;
        else scrollOffset.y *= 0.8;
        if(fabs(scrollOffset.x) < 1) scrollOffset.x = 0;
        else scrollOffset.x *= 0.8;
#else
        scrollOffset.x  = 0;
        scrollOffset.y  = 0;
#endif
        pointerOffset.x = 0;
        pointerOffset.y = 0; });
}

short GLFWInputManager::getControllersConnectedCount()
{
    return controllersCount;
}

void GLFWInputManager::updateUnifiedControllerState(ControllerState* state)
{
    for (size_t i = 0; i < _BUTTON_MAX; i++)
        state->buttons[i] = false;

    for (size_t i = 0; i < _AXES_MAX; i++)
        state->axes[i] = 0;

    if (!glfwGetWindowAttrib(window, GLFW_FOCUSED))
        return;

    for (int i = 0; i < GAMEPADS_MAX; i++)
    {
        ControllerState localState {};
        updateControllerState(&localState, i);

        for (size_t i = 0; i < _BUTTON_MAX; i++)
            state->buttons[i] |= localState.buttons[i];

        for (size_t i = 0; i < _AXES_MAX; i++)
        {
            state->axes[i] += localState.axes[i];

            if (state->axes[i] < -1)
                state->axes[i] = -1;
            else if (state->axes[i] > 1)
                state->axes[i] = 1;
        }
    }

    // Add keyboard keys on top of gamepad buttons
    if (Application::isSwapInputKeys()) {
        state->buttons[BUTTON_B] |= glfwGetKey(this->window, GLFW_KEY_KP_ENTER) != 0;
        state->buttons[BUTTON_B] |= glfwGetKey(this->window, GLFW_KEY_ENTER) != 0;
        state->buttons[BUTTON_A] |= glfwGetKey(this->window, GLFW_KEY_ESCAPE) != 0;
        state->buttons[BUTTON_A] |= glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    } else {
        state->buttons[BUTTON_A] |= glfwGetKey(this->window, GLFW_KEY_KP_ENTER) != 0;
        state->buttons[BUTTON_A] |= glfwGetKey(this->window, GLFW_KEY_ENTER) != 0;
        state->buttons[BUTTON_B] |= glfwGetKey(this->window, GLFW_KEY_ESCAPE) != 0;
        state->buttons[BUTTON_B] |= glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    }
    state->buttons[BUTTON_X] |= (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS);

    state->buttons[BUTTON_UP] |= glfwGetKey(this->window, GLFW_KEY_UP) != 0;
    state->buttons[BUTTON_RIGHT] |= glfwGetKey(this->window, GLFW_KEY_RIGHT) != 0;
    state->buttons[BUTTON_DOWN] |= glfwGetKey(this->window, GLFW_KEY_DOWN) != 0;
    state->buttons[BUTTON_LEFT] |= glfwGetKey(this->window, GLFW_KEY_LEFT) != 0;

    state->buttons[BUTTON_NAV_UP] |= state->buttons[BUTTON_UP];
    state->buttons[BUTTON_NAV_RIGHT] |= state->buttons[BUTTON_RIGHT];
    state->buttons[BUTTON_NAV_DOWN] |= state->buttons[BUTTON_DOWN];
    state->buttons[BUTTON_NAV_LEFT] |= state->buttons[BUTTON_LEFT];
}

void GLFWInputManager::updateControllerState(ControllerState* state, int controller)
{
    // Get gamepad state
    GLFWgamepadstate glfwState = {};
    glfwGetGamepadState(controller, &glfwState);

    for (size_t i = 0; i < GLFW_GAMEPAD_BUTTON_MAX; i++)
    {
        // Translate GLFW gamepad to borealis controller
        size_t brlsButton          = GLFW_BUTTONS_MAPPING[i];
        state->buttons[brlsButton] = (bool)glfwState.buttons[i];
    }

    state->buttons[BUTTON_LT] = glfwState.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER] > 0.1f;
    state->buttons[BUTTON_RT] = glfwState.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] > 0.1f;

    state->buttons[BUTTON_NAV_UP]    = glfwState.axes[GLFW_GAMEPAD_AXIS_LEFT_Y] < -0.5f || glfwState.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y] < -0.5f || state->buttons[BUTTON_UP];
    state->buttons[BUTTON_NAV_RIGHT] = glfwState.axes[GLFW_GAMEPAD_AXIS_LEFT_X] > 0.5f || glfwState.axes[GLFW_GAMEPAD_AXIS_RIGHT_X] > 0.5f || state->buttons[BUTTON_RIGHT];
    state->buttons[BUTTON_NAV_DOWN]  = glfwState.axes[GLFW_GAMEPAD_AXIS_LEFT_Y] > 0.5f || glfwState.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y] > 0.5f || state->buttons[BUTTON_DOWN];
    state->buttons[BUTTON_NAV_LEFT]  = glfwState.axes[GLFW_GAMEPAD_AXIS_LEFT_X] < -0.5f || glfwState.axes[GLFW_GAMEPAD_AXIS_RIGHT_X] < -0.5f || state->buttons[BUTTON_LEFT];

    for (size_t i = 0; i < GLFW_GAMEPAD_AXIS_MAX; i++)
    {
        state->axes[GLFW_AXIS_MAPPING[i]] = glfwState.axes[i];
    }
}

bool GLFWInputManager::getKeyboardKeyState(BrlsKeyboardScancode key)
{
    return glfwGetKey(this->window, key);
}

void GLFWInputManager::updateTouchStates(std::vector<RawTouchState>* states)
{
    if (getTouchState()) {
        touchState.pressed = true;
        states->push_back(touchState);
    }
}

void GLFWInputManager::updateMouseStates(RawMouseState* state)
{
    double x, y;
    glfwGetCursorPos(this->window, &x, &y);

    state->leftButton   = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    state->middleButton = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;
    state->rightButton  = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

#if defined(BOREALIS_USE_METAL) || defined(BOREALIS_USE_D3D11)
    // 使用 metal, d3d11 的 cocoa 窗口鼠标事件不需要进行 dpi 缩放。
    state->position.x = x / Application::windowScale;
    state->position.y = y / Application::windowScale;
#else
    double scaleFactor = brls::Application::getPlatform()->getVideoContext()->getScaleFactor();
    state->position.x  = x * scaleFactor / Application::windowScale;
    state->position.y  = y * scaleFactor / Application::windowScale;
#endif
    state->offset = pointerOffset;
    state->scroll = scrollOffset;
}

void GLFWInputManager::setPointerLock(bool lock)
{
    pointerLocked = lock;

    int state = lock ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL;
    glfwSetInputMode(window, GLFW_CURSOR, state);
}

void GLFWInputManager::runloopStart()
{
    pointerOffset         = pointerOffsetBuffer;
    pointerOffsetBuffer.x = 0;
    pointerOffsetBuffer.y = 0;
}

void GLFWInputManager::sendRumble(unsigned short controller, unsigned short lowFreqMotor, unsigned short highFreqMotor)
{
}

};
