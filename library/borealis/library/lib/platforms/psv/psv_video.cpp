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

#include <borealis/core/application.hpp>
#include <borealis/platforms/psv/psv_video.hpp>

#define NANOVG_GXM_IMPLEMENTATION
#define NANOVG_GXM_UTILS_IMPLEMENTATION
#include <nanovg_gxm.h>

namespace brls
{

PsvVideoContext::PsvVideoContext()
{
#ifdef USE_VITA_SHARK
    if (shark_init("app0:module/libshacccg.suprx") < 0) {
        sceClibPrintf("vitashark: failed to initialize");
        return;
    }
#endif

    // Initialize nanovg
    NVGXMinitOptions initOptions = {
        .msaa = SCE_GXM_MULTISAMPLE_4X,
        .swapInterval = VideoContext::swapInterval,
        .dumpShader = 0,
        .scenesPerFrame = 1,
    };

    window = gxmCreateWindow(&initOptions);
    if (window == NULL) {
        fatal("gxm: failed to initialize");
    }

    this->nvgContext = nvgCreateGXM(window->context, window->shader_patcher, 0);
    if (!this->nvgContext)
    {
        brls::fatal("gxm: unable to init nanovg");
    }

    Application::setWindowSize(DISPLAY_WIDTH, DISPLAY_HEIGHT);
}

PsvVideoContext::~PsvVideoContext()
{
    try
    {
        if (this->nvgContext)
        {
            nvgDeleteGXM(this->nvgContext);
            gxmDeleteWindow(window);
        }
    }
    catch (...)
    {
        Logger::error("Cannot delete nvg Context");
    }
#ifdef USE_VITA_SHARK
    shark_end();
#endif
}

void PsvVideoContext::clear(NVGcolor color) {
    gxmClearColor(color.r, color.g, color.b, color.a);
}

void PsvVideoContext::resetState() { }

void PsvVideoContext::beginFrame() {
    gxmBeginFrame();
    gxmClear();
}

void PsvVideoContext::endFrame() {
    gxmEndFrame();
    gxmDialogUpdate();
    gxmSwapBuffer();
}

void PsvVideoContext::setSwapInterval(int interval) {
    VideoContext::swapInterval = interval;
    gxmSwapInterval(interval);
}

double PsvVideoContext::getScaleFactor() {
    return 1.0;
}

NVGcontext* PsvVideoContext::getNVGContext() {
    return this->nvgContext;
}

NVGXMwindow* PsvVideoContext::getWindow()
{
    return this->window;
}

}