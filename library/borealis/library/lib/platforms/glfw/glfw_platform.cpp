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

#include <borealis/core/application.hpp>
#include <borealis/core/i18n.hpp>
#include <borealis/core/logger.hpp>
#include <borealis/platforms/glfw/glfw_platform.hpp>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <strings.h>

// glfw video and input code inspired from the glfw hybrid app by fincs
// https://github.com/fincs/hybrid_app

namespace brls
{

#if defined(__APPLE__)
std::string darwin_locale();
#endif

static void glfwErrorCallback(int errorCode, const char* description)
{
    switch (errorCode)
    {
        case GLFW_API_UNAVAILABLE:
            Logger::error("OpenGL is unavailable: {}", description);
            break;
        case GLFW_VERSION_UNAVAILABLE:
            Logger::error("OpenGL 3.2 (the minimum requirement) is not available: {}", description);
            break;
        default:
            Logger::error("GLFW {}: {}", errorCode, description);
    }
}

GLFWPlatform::GLFWPlatform()
{
    // Init glfw
    glfwSetErrorCallback(glfwErrorCallback);
    glfwInitHint(GLFW_JOYSTICK_HAT_BUTTONS, GLFW_TRUE);

    if (!glfwInit())
    {
        Logger::error("glfw: failed to initialize");
        return;
    }

    // Misc
    glfwSetTime(0.0);

    // override local
    if (Platform::APP_LOCALE_DEFAULT == LOCALE_AUTO)
    {
#if defined(_WIN32)
        LANGID lang = GetUserDefaultLangID();
        switch (PRIMARYLANGID(lang)) {
        case LANG_CHINESE: this->locale = (SUBLANGID(lang) == SUBLANG_CHINESE_SIMPLIFIED) ? LOCALE_ZH_HANS : LOCALE_ZH_HANT; break;
        case LANG_ENGLISH: this->locale = (SUBLANGID(lang) == SUBLANG_ENGLISH_UK) ? LOCALE_EN_GB : LOCALE_EN_US; break;
        case LANG_FRENCH: this->locale = (SUBLANGID(lang) == SUBLANG_FRENCH_CANADIAN) ? LOCALE_FR_CA : LOCALE_FR; break;
        case LANG_PORTUGUESE: this->locale = (SUBLANGID(lang) == SUBLANG_PORTUGUESE_BRAZILIAN) ? LOCALE_PT_BR : LOCALE_PT; break;
        case LANG_JAPANESE: this->locale = LOCALE_JA; break;
        case LANG_GERMAN: this->locale = LOCALE_DE; break;
        case LANG_ITALIAN: this->locale = LOCALE_IT; break;
        case LANG_SPANISH: this->locale = LOCALE_ES; break;
        case LANG_KOREAN: this->locale = LOCALE_Ko; break;
        case LANG_DUTCH: this->locale = LOCALE_NL; break;
        case LANG_RUSSIAN: this->locale = LOCALE_RU; break;
        default:;
        }
#elif defined(__APPLE__)
        std::string lang = darwin_locale();
        auto pos = lang.find_last_of('-');
        this->locale = lang.substr(0, pos);
#elif defined(__linux__)
        char* langEnv = getenv("LANG");
        /* fallback languages */
        if (!langEnv) langEnv = getenv("LANGUAGE");
        if (langEnv) {
            std::unordered_map<std::string, std::string> lang2brls = {
                { "zh_CN", LOCALE_ZH_HANS },
                { "zh_TW", LOCALE_ZH_HANT },
                { "ja_JP", LOCALE_JA },
                { "ko_KR", LOCALE_Ko },
                { "it_IT", LOCALE_IT }
            };
            char *ptr = strchr(langEnv, '.');
            if (ptr) *ptr = '\0';

            ptr = strchr(langEnv, '@');
            if (ptr) *ptr = '\0';

            std::string lang = langEnv;
            this->locale = lang2brls.count(lang) ? lang2brls[lang] : LOCALE_EN_US;
        }
#endif
        brls::Logger::info("Auto detect app locale: {}", this->locale);
    }

    // Platform impls
    this->audioPlayer = new NullAudioPlayer();
}

void GLFWPlatform::createWindow(std::string windowTitle, uint32_t windowWidth, uint32_t windowHeight, float windowXPos, float windowYPos)
{
    this->videoContext = new GLFWVideoContext(windowTitle, windowWidth, windowHeight, windowXPos, windowYPos);
    GLFWwindow* win    = this->videoContext->getGLFWWindow();
    this->inputManager = new GLFWInputManager(win);
    this->imeManager   = new GLFWImeManager(win);
}

void GLFWPlatform::setWindowAlwaysOnTop(bool enable)
{
    glfwSetWindowAttrib(this->videoContext->getGLFWWindow(), GLFW_FLOATING, enable ? GLFW_TRUE : GLFW_FALSE);
}

void GLFWPlatform::restoreWindow()
{
    glfwRestoreWindow(this->videoContext->getGLFWWindow());
}

void GLFWPlatform::setWindowSize(uint32_t windowWidth, uint32_t windowHeight)
{
    if (windowWidth > 0 && windowHeight > 0)
    {
        glfwSetWindowSize(this->videoContext->getGLFWWindow(), windowWidth, windowHeight);
    }
}

void GLFWPlatform::setWindowSizeLimits(uint32_t windowMinWidth, uint32_t windowMinHeight, uint32_t windowMaxWidth, uint32_t windowMaxHeight)
{
    if (windowMinWidth > 0 && windowMinHeight > 0)
        glfwSetWindowSizeLimits(this->videoContext->getGLFWWindow(), windowMinWidth, windowMinHeight, GLFW_DONT_CARE, GLFW_DONT_CARE);
    if ((windowMaxWidth > 0 && windowMaxHeight > 0) && (windowMaxHeight > windowMinWidth && windowMaxHeight > windowMinHeight))
        glfwSetWindowSizeLimits(this->videoContext->getGLFWWindow(), GLFW_DONT_CARE, GLFW_DONT_CARE, windowMaxWidth, windowMaxHeight);
}

void GLFWPlatform::setWindowPosition(int windowXPos, int windowYPos)
{
    glfwSetWindowPos(this->videoContext->getGLFWWindow(), windowXPos, windowYPos);
}

void GLFWPlatform::setWindowState(uint32_t windowWidth, uint32_t windowHeight, int windowXPos, int windowYPos)
{
    if (windowWidth > 0 && windowHeight > 0)
    {
        GLFWwindow* win = this->videoContext->getGLFWWindow();
        glfwRestoreWindow(win);
        glfwSetWindowMonitor(win, nullptr, windowXPos, windowYPos, windowWidth, windowHeight, 0);
    }
}

void GLFWPlatform::pasteToClipboard(const std::string& text)
{
    glfwSetClipboardString(this->videoContext->getGLFWWindow(), text.c_str());
}

std::string GLFWPlatform::pasteFromClipboard()
{
    const char* str = glfwGetClipboardString(this->videoContext->getGLFWWindow());
    if (!str)
        return "";
    return std::string { str };
}

std::string GLFWPlatform::getName()
{
    return "GLFW";
}

bool GLFWPlatform::mainLoopIteration()
{
    bool isActive;
    do
    {
        isActive = !glfwGetWindowAttrib(this->videoContext->getGLFWWindow(), GLFW_ICONIFIED);

        if (isActive)
        {
            glfwPollEvents();
            if (!Application::hasActiveEvent())
            {
                glfwWaitEventsTimeout(Application::getDeactivatedFrameTime());
            }
        }
        else
        {
            glfwWaitEvents();
        }
    } while (!isActive);

    return !glfwWindowShouldClose(this->videoContext->getGLFWWindow());
}

AudioPlayer* GLFWPlatform::getAudioPlayer()
{
    return this->audioPlayer;
}

VideoContext* GLFWPlatform::getVideoContext()
{
    return this->videoContext;
}

InputManager* GLFWPlatform::getInputManager()
{
    return this->inputManager;
}

ImeManager* GLFWPlatform::getImeManager()
{
    return this->imeManager;
}

GLFWPlatform::~GLFWPlatform()
{
    delete this->audioPlayer;
    delete this->inputManager;
    delete this->imeManager;
    delete this->videoContext;
}

} // namespace brls
