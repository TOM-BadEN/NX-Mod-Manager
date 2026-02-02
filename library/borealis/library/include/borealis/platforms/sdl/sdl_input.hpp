#pragma once

#include <borealis/core/input.hpp>

#include <SDL2/SDL.h>

namespace brls
{

// Input manager for GLFW gamepad and keyboard
class SDLInputManager : public InputManager
{
  public:
    explicit SDLInputManager(SDL_Window* window);

    ~SDLInputManager();

    short getControllersConnectedCount() override;

    void updateUnifiedControllerState(ControllerState* state) override;

    void updateControllerState(ControllerState* state, int controller) override;

    bool getKeyboardKeyState(BrlsKeyboardScancode state) override;

    void updateTouchStates(std::vector<RawTouchState>* states) override;

    void updateMouseStates(RawMouseState* state) override;

    void sendRumble(unsigned short controller, unsigned short lowFreqMotor, unsigned short highFreqMotor) override;

    void runloopStart() override;

    void setPointerLock(bool lock) override;

    void updateMouseMotion(SDL_MouseMotionEvent event);

    void updateMouseWheel(SDL_MouseWheelEvent event);

    void updateControllerSensorsUpdate(SDL_ControllerSensorEvent event);

    void updateKeyboardState(SDL_KeyboardEvent event);

  private:
    Point scrollOffset;
    Point pointerOffset;
    Point pointerOffsetBuffer;
    bool pointerLocked = false;

    SDL_Window* window;
};

};
