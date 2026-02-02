/*
    Copyright 2021 natinusala
    Copyright (C) 2021  XITRIX

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

#include <switch.h>
#include <unordered_map>

#include <borealis/core/input.hpp>

#define TOUCHES_MAX 10

namespace brls
{

// InputManager that uses the hid sysmodule to get inputs
class SwitchInputManager : public InputManager
{
  public:
    SwitchInputManager();
    ~SwitchInputManager() override;

    short getControllersConnectedCount() override;

    void updateUnifiedControllerState(ControllerState* state) override;

    void updateControllerState(ControllerState* state, int controller) override;

    bool getKeyboardKeyState(BrlsKeyboardScancode key) override;

    void updateTouchStates(std::vector<RawTouchState>* states) override;

    void updateMouseStates(RawMouseState* state) override;

    void sendRumble(unsigned short controller, unsigned short lowFreqMotor, unsigned short highFreqMotor) override;

    void runloopStart() override;

    void setPointerLock(bool lock) override;

    void drawCursor(NVGcontext* vg) override;

    void sendRumbleRaw(float lowFreq, float highFreq, float lowAmp, float highAmp);

    void clearVibration(int controller);

  private:
    bool cursorInited = false;
    int cursorWidth, cursorHeight;
    int cursorTexture = 0;
    NVGpaint paint;
    Point lastCursorPosition;
    PadState padStateHandheld;
    HidVibrationDeviceHandle m_vibration_device_handheld[2];
    HidVibrationValue m_vibration_values_handheld[2];
    PadState padsState[GAMEPADS_MAX];
    HidVibrationDeviceHandle m_vibration_device_handles[GAMEPADS_MAX][2];
    HidVibrationValue m_vibration_values[GAMEPADS_MAX][2];
    u32 padsStyleSet[GAMEPADS_MAX];
    bool pointerLocked = false;
    HidMouseState currentMouseState;
    HidSixAxisSensorHandle m_six_axis_sensor_handle[4]; // All for player 1, player 2 not supported

    std::vector<bool> m_hid_keyboard_state;
    std::unordered_map<int, BrlsKeyboardScancode> m_hid_brls_map;
    std::unordered_map<BrlsKeyboardScancode, int> m_brls_hid_map;

    void initCursor(NVGcontext* vg);
    void handleMouse();
    void handleKeyboard();
    void handleControllerSensors();
    void upToDateMouseState();
    void hidInitializeKeyboardMap();
    void updateControllerStateInner(ControllerState* state, PadState* pad);
    void sendRumbleInternal(HidVibrationDeviceHandle vibration_device[2], HidVibrationValue vibration_values[2],
        unsigned short lowFreqMotor, unsigned short highFreqMotor);
    void sendRumbleInternal(HidVibrationDeviceHandle vibration_device[2], HidVibrationValue vibration_values[2],
        float lowFreq, float highFreq, float lowAmp, float highAmp);

};

/// HidKeyboardScancode
typedef enum
{
    KBD_NONE    = 0x00,
    KBD_ERR_OVF = 0x01,

    KBD_A = 0x04,
    KBD_B = 0x05,
    KBD_C = 0x06,
    KBD_D = 0x07,
    KBD_E = 0x08,
    KBD_F = 0x09,
    KBD_G = 0x0a,
    KBD_H = 0x0b,
    KBD_I = 0x0c,
    KBD_J = 0x0d,
    KBD_K = 0x0e,
    KBD_L = 0x0f,
    KBD_M = 0x10,
    KBD_N = 0x11,
    KBD_O = 0x12,
    KBD_P = 0x13,
    KBD_Q = 0x14,
    KBD_R = 0x15,
    KBD_S = 0x16,
    KBD_T = 0x17,
    KBD_U = 0x18,
    KBD_V = 0x19,
    KBD_W = 0x1a,
    KBD_X = 0x1b,
    KBD_Y = 0x1c,
    KBD_Z = 0x1d,

    KBD_1 = 0x1e,
    KBD_2 = 0x1f,
    KBD_3 = 0x20,
    KBD_4 = 0x21,
    KBD_5 = 0x22,
    KBD_6 = 0x23,
    KBD_7 = 0x24,
    KBD_8 = 0x25,
    KBD_9 = 0x26,
    KBD_0 = 0x27,

    KBD_ENTER      = 0x28,
    KBD_ESC        = 0x29,
    KBD_BACKSPACE  = 0x2a,
    KBD_TAB        = 0x2b,
    KBD_SPACE      = 0x2c,
    KBD_MINUS      = 0x2d,
    KBD_EQUAL      = 0x2e,
    KBD_LEFTBRACE  = 0x2f,
    KBD_RIGHTBRACE = 0x30,
    KBD_BACKSLASH  = 0x31,
    KBD_HASHTILDE  = 0x32,
    KBD_SEMICOLON  = 0x33,
    KBD_APOSTROPHE = 0x34,
    KBD_GRAVE      = 0x35,
    KBD_COMMA      = 0x36,
    KBD_DOT        = 0x37,
    KBD_SLASH      = 0x38,
    KBD_CAPSLOCK   = 0x39,

    KBD_F1  = 0x3a,
    KBD_F2  = 0x3b,
    KBD_F3  = 0x3c,
    KBD_F4  = 0x3d,
    KBD_F5  = 0x3e,
    KBD_F6  = 0x3f,
    KBD_F7  = 0x40,
    KBD_F8  = 0x41,
    KBD_F9  = 0x42,
    KBD_F10 = 0x43,
    KBD_F11 = 0x44,
    KBD_F12 = 0x45,

    KBD_SYSRQ      = 0x46,
    KBD_SCROLLLOCK = 0x47,
    KBD_PAUSE      = 0x48,
    KBD_INSERT     = 0x49,
    KBD_HOME       = 0x4a,
    KBD_PAGEUP     = 0x4b,
    KBD_DELETE     = 0x4c,
    KBD_END        = 0x4d,
    KBD_PAGEDOWN   = 0x4e,
    KBD_RIGHT      = 0x4f,
    KBD_LEFT       = 0x50,
    KBD_DOWN       = 0x51,
    KBD_UP         = 0x52,

    KBD_NUMLOCK    = 0x53,
    KBD_KPSLASH    = 0x54,
    KBD_KPASTERISK = 0x55,
    KBD_KPMINUS    = 0x56,
    KBD_KPPLUS     = 0x57,
    KBD_KPENTER    = 0x58,
    KBD_KP1        = 0x59,
    KBD_KP2        = 0x5a,
    KBD_KP3        = 0x5b,
    KBD_KP4        = 0x5c,
    KBD_KP5        = 0x5d,
    KBD_KP6        = 0x5e,
    KBD_KP7        = 0x5f,
    KBD_KP8        = 0x60,
    KBD_KP9        = 0x61,
    KBD_KP0        = 0x62,
    KBD_KPDOT      = 0x63,

    KBD_102ND   = 0x64,
    KBD_COMPOSE = 0x65,
    KBD_POWER   = 0x66,
    KBD_KPEQUAL = 0x67,

    KBD_F13 = 0x68,
    KBD_F14 = 0x69,
    KBD_F15 = 0x6a,
    KBD_F16 = 0x6b,
    KBD_F17 = 0x6c,
    KBD_F18 = 0x6d,
    KBD_F19 = 0x6e,
    KBD_F20 = 0x6f,
    KBD_F21 = 0x70,
    KBD_F22 = 0x71,
    KBD_F23 = 0x72,
    KBD_F24 = 0x73,

    KBD_OPEN              = 0x74,
    KBD_HELP              = 0x75,
    KBD_PROPS             = 0x76,
    KBD_FRONT             = 0x77,
    KBD_STOP              = 0x78,
    KBD_AGAIN             = 0x79,
    KBD_UNDO              = 0x7a,
    KBD_CUT               = 0x7b,
    KBD_COPY              = 0x7c,
    KBD_PASTE             = 0x7d,
    KBD_FIND              = 0x7e,
    KBD_MUTE              = 0x7f,
    KBD_VOLUMEUP          = 0x80,
    KBD_VOLUMEDOWN        = 0x81,
    KBD_CAPSLOCK_ACTIVE   = 0x82,
    KBD_NUMLOCK_ACTIVE    = 0x83,
    KBD_SCROLLLOCK_ACTIVE = 0x84,
    KBD_KPCOMMA           = 0x85,

    KBD_KPLEFTPAREN  = 0xb6,
    KBD_KPRIGHTPAREN = 0xb7,

    KBD_LEFTCTRL   = 0xe0,
    KBD_LEFTSHIFT  = 0xe1,
    KBD_LEFTALT    = 0xe2,
    KBD_LEFTMETA   = 0xe3,
    KBD_RIGHTCTRL  = 0xe4,
    KBD_RIGHTSHIFT = 0xe5,
    KBD_RIGHTALT   = 0xe6,
    KBD_RIGHTMETA  = 0xe7,

    KBD_MEDIA_PLAYPAUSE    = 0xe8,
    KBD_MEDIA_STOPCD       = 0xe9,
    KBD_MEDIA_PREVIOUSSONG = 0xea,
    KBD_MEDIA_NEXTSONG     = 0xeb,
    KBD_MEDIA_EJECTCD      = 0xec,
    KBD_MEDIA_VOLUMEUP     = 0xed,
    KBD_MEDIA_VOLUMEDOWN   = 0xee,
    KBD_MEDIA_MUTE         = 0xef,
    KBD_MEDIA_WWW          = 0xf0,
    KBD_MEDIA_BACK         = 0xf1,
    KBD_MEDIA_FORWARD      = 0xf2,
    KBD_MEDIA_STOP         = 0xf3,
    KBD_MEDIA_FIND         = 0xf4,
    KBD_MEDIA_SCROLLUP     = 0xf5,
    KBD_MEDIA_SCROLLDOWN   = 0xf6,
    KBD_MEDIA_EDIT         = 0xf7,
    KBD_MEDIA_SLEEP        = 0xf8,
    KBD_MEDIA_COFFEE       = 0xf9,
    KBD_MEDIA_REFRESH      = 0xfa,
    KBD_MEDIA_CALC         = 0xfb
} HidKeyboardScancode;

} // namespace brls
