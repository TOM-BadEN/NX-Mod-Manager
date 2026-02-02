/*
    Copyright 2021 natinusala

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

#include <borealis/core/platform.hpp>

#ifdef __SWITCH__
#include <borealis/platforms/switch/switch_platform.hpp>
#endif

#ifdef ANDROID
#include <borealis/platforms/android/android_platform.hpp>
#endif

#ifdef __PSV__
#include <borealis/platforms/psv/psv_platform.hpp>
#endif

#ifdef PS4
#include <borealis/platforms/ps4/ps4_platform.hpp>
#endif

#if defined(BOREALIS_USE_DEKO3D)
#elif defined(__GLFW__)
#include <borealis/platforms/glfw/glfw_platform.hpp>
#elif defined(__SDL2__)
#include <borealis/platforms/sdl/sdl_platform.hpp>
#endif

namespace brls
{

Platform* Platform::createPlatform()
{
#if defined(__SWITCH__)
    return new SwitchPlatform();
#elif defined(ANDROID)
    return new AndroidPlatform();
#elif defined(__PSV__)
    return new PsvPlatform();
#elif defined(PS4)
    return new Ps4Platform();
#elif defined(__SDL2__)
    return new SDLPlatform();
#elif defined(__GLFW__)
    return new GLFWPlatform();
#endif

    return nullptr;
}

} // namespace brls
