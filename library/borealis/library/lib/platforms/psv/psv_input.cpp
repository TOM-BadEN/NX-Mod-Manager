/*
Copyright 2024 xfangfang

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

#include <psp2/ctrl.h>
#include <psp2/touch.h>

#include <borealis/core/application.hpp>
#include <borealis/core/logger.hpp>
#include <borealis/platforms/psv/psv_input.hpp>

namespace brls
{

PsvInputManager::PsvInputManager()
{
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
    memset(&pad, 0, sizeof(pad));

    SceTouchPanelInfo panelInfo;
    sceTouchGetPanelInfo(SCE_TOUCH_PORT_FRONT, &panelInfo);

    screen.origin.x    = panelInfo.minAaX;
    screen.origin.y    = panelInfo.minAaY;
    screen.size.width  = panelInfo.maxAaX - panelInfo.minAaX;
    screen.size.height = panelInfo.maxAaY - panelInfo.minAaY;

    sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);
}

PsvInputManager::~PsvInputManager() = default;

Point PsvInputManager::convertVitaTouchXY(float vita_x, float vita_y) const
{
    float x = (vita_x - screen.origin.x) / screen.size.width;
    float y = (vita_y - screen.origin.y) / screen.size.height;

    x = SDL_max(x, 0.0);
    x = SDL_min(x, 1.0);

    y = SDL_max(y, 0.0);
    y = SDL_min(y, 1.0);

    return { Application::contentWidth * x, Application::contentHeight * y };
}

short PsvInputManager::getControllersConnectedCount()
{
    return 1;
}

void PsvInputManager::updateUnifiedControllerState(ControllerState* state)
{
    this->updateControllerState(state, 0);
}

void PsvInputManager::updateControllerState(ControllerState* state, int controller)
{
    for (bool& button : state->buttons)
        button = false;

    for (float& axe : state->axes)
        axe = 0;

    sceCtrlPeekBufferPositive(0, &pad, 1);

    state->buttons[BUTTON_UP]    = pad.buttons & SCE_CTRL_UP;
    state->buttons[BUTTON_RIGHT] = pad.buttons & SCE_CTRL_RIGHT;
    state->buttons[BUTTON_DOWN]  = pad.buttons & SCE_CTRL_DOWN;
    state->buttons[BUTTON_LEFT]  = pad.buttons & SCE_CTRL_LEFT;

    state->buttons[BUTTON_START] = pad.buttons & SCE_CTRL_START;
    state->buttons[BUTTON_BACK]  = pad.buttons & SCE_CTRL_SELECT;

    state->buttons[BUTTON_LB] = pad.buttons & SCE_CTRL_L2;
    state->buttons[BUTTON_RB] = pad.buttons & SCE_CTRL_R2;

    state->buttons[BUTTON_A] = pad.buttons & SCE_CTRL_CROSS;
    state->buttons[BUTTON_B] = pad.buttons & SCE_CTRL_CIRCLE;
    state->buttons[BUTTON_X] = pad.buttons & SCE_CTRL_SQUARE;
    state->buttons[BUTTON_Y] = pad.buttons & SCE_CTRL_TRIANGLE;

    state->axes[LEFT_X]  = pad.lx / 255.0f - 1.0f;
    state->axes[LEFT_Y]  = pad.ly / 255.0f - 1.0f;
    state->axes[RIGHT_X] = pad.rx / 255.0f - 1.0f;
    state->axes[RIGHT_Y] = pad.ry / 255.0f - 1.0f;

    state->buttons[BUTTON_NAV_UP]    = pad.ly < 0x20 || pad.ry < 0x20 || state->buttons[BUTTON_UP];
    state->buttons[BUTTON_NAV_RIGHT] = pad.lx > 0xe0 || pad.rx > 0xe0 || state->buttons[BUTTON_RIGHT];
    state->buttons[BUTTON_NAV_DOWN]  = pad.ly > 0xe0 || pad.ry > 0xe0 || state->buttons[BUTTON_DOWN];
    state->buttons[BUTTON_NAV_LEFT]  = pad.lx < 0x20 || pad.rx < 0x20 || state->buttons[BUTTON_LEFT];
}

bool PsvInputManager::getKeyboardKeyState(BrlsKeyboardScancode key)
{
    return false;
}

void PsvInputManager::updateTouchStates(std::vector<RawTouchState>* states)
{
    SceTouchData touch;
    sceTouchPeek(SCE_TOUCH_PORT_FRONT, &touch, 1);

    for (int i = 0; i < touch.reportNum; i++)
    {
        Point p = convertVitaTouchXY(touch.report[i].x, touch.report[i].y);
        RawTouchState state;
        state.pressed    = true;
        state.fingerId   = touch.report[i].id;
        state.position.x = p.x;
        state.position.y = p.y;
        states->push_back(state);
    }
}

void PsvInputManager::updateMouseStates(RawMouseState* state)
{
}

void PsvInputManager::setPointerLock(bool lock)
{
}

void PsvInputManager::runloopStart()
{
}

void PsvInputManager::sendRumble(unsigned short controller, unsigned short lowFreqMotor, unsigned short highFreqMotor)
{
}

void PsvInputManager::updateMouseMotion(SDL_MouseMotionEvent event)
{
}

void PsvInputManager::updateMouseWheel(SDL_MouseWheelEvent event)
{
}

void PsvInputManager::updateControllerSensorsUpdate(SDL_ControllerSensorEvent event)
{
}

void PsvInputManager::updateKeyboardState(SDL_KeyboardEvent event)
{
}

};
