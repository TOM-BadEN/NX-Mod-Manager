#pragma once

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3native.h>

#include <nanovg_mtl.h>

// 创建一个 metal 上下文
void *CreateMetalContext(GLFWwindow* window);
// 从 metal 上下文里取出 MetalLayer
void *GetMetalLayer(void *ctx);
// MetalLayer 画布缩放
void ResizeMetalDrawable(void *ctx, int width, int height);
// 获取窗口 dpi
float GetMetalScaleFactor(void *ctx);
