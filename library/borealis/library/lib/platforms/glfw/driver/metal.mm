#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>
#include <borealis/platforms/glfw/driver/metal.hpp>


struct context_mtl {
	id<MTLDevice> device;
	CAMetalLayer* layer;
    NSWindow* win;
};

void *GetMetalLayer(void *ctx) {
    struct context_mtl *c = (struct context_mtl *)ctx;
    return (__bridge void *)c->layer;
}

void *CreateMetalContext(GLFWwindow* window) {
    struct context_mtl* mtl = (struct context_mtl*)calloc(1, sizeof(struct context_mtl));
    NSWindow* nswin = glfwGetCocoaWindow(window);
    mtl->win = nswin;
    mtl->device = MTLCreateSystemDefaultDevice();
    mtl->layer = [CAMetalLayer layer];
    mtl->layer.device = mtl->device;
    mtl->layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    nswin.contentView.layer = mtl->layer;
    nswin.contentView.wantsLayer = YES;
    return (void *)mtl;
}

void ResizeMetalDrawable(void *ctx, int width, int height) {
    struct context_mtl *c = (struct context_mtl *)ctx;
    CGSize sz;
    sz.width = width;
    sz.height = height;
    c->layer.drawableSize = sz;
}

float GetMetalScaleFactor(void *ctx) {
    struct context_mtl *c = (struct context_mtl *)ctx;
    float scaleFactor = [c->win backingScaleFactor];
    return scaleFactor;
}
