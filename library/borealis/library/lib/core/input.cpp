/*
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
#include <borealis/core/input.hpp>

namespace brls
{

TouchPhase getPhase(bool oldState, bool newState)
{
    if (!oldState && newState)
        return TouchPhase::START;

    if (oldState && newState)
        return TouchPhase::STAY;

    if (oldState && !newState)
        return TouchPhase::END;

    return TouchPhase::NONE;
}

TouchPhase getPhase(TouchPhase oldState, bool newState)
{
    bool old = oldState == TouchPhase::START || oldState == TouchPhase::STAY;
    return getPhase(old, newState);
}

TouchState InputManager::computeTouchState(RawTouchState currentTouch, TouchState lastFrameState)
{
    TouchState state;
    state.fingerId = lastFrameState.fingerId;
    state.view     = lastFrameState.view;
    state.phase    = getPhase(lastFrameState.phase, currentTouch.pressed);
    if (state.phase == TouchPhase::END)
        state.position = lastFrameState.position;
    else
        state.position = currentTouch.position;
    return state;
}

MouseState InputManager::computeMouseState(RawMouseState currentTouch, MouseState lastFrameState)
{
    MouseState state;
    state.view         = lastFrameState.view;
    state.position     = currentTouch.position;
    state.offset       = currentTouch.offset;
    state.scroll       = currentTouch.scroll;
    state.leftButton   = getPhase(lastFrameState.leftButton, currentTouch.leftButton);
    state.middleButton = getPhase(lastFrameState.middleButton, currentTouch.middleButton);
    state.rightButton  = getPhase(lastFrameState.rightButton, currentTouch.rightButton);
    return state;
}

ControllerButton InputManager::mapControllerState(ControllerButton button)
{
    if (!Application::isSwapInputKeys())
        return button;

    switch (button)
    {
        case BUTTON_A:
            return BUTTON_B;
        case BUTTON_B:
            return BUTTON_A;
        case BUTTON_X:
            return BUTTON_Y;
        case BUTTON_Y:
            return BUTTON_X;
        default:
            return button;
    }
}

} // namespace brls
