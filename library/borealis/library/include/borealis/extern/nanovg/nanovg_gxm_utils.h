//
// Copyright (c) 2024 xfangfang xfangfang@126.com
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//

#ifndef NANOVG_GXM_UTILS_H
#define NANOVG_GXM_UTILS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <psp2/gxm.h>
#include <psp2/kernel/sysmem.h>

#ifdef USE_VITA_SHARK

#include <vitashark.h>
#include <stdio.h>

#endif

#define ALIGN(x, a) (((x) + ((a) - 1)) & ~((a) - 1))

// Default framebuffer settings
#define DISPLAY_WIDTH 960
#define DISPLAY_HEIGHT 544
#define DISPLAY_STRIDE 960
#define DISPLAY_BUFFER_COUNT 3
#define MAX_PENDING_SWAPS (DISPLAY_BUFFER_COUNT - 1)

#define DISPLAY_COLOR_FORMAT SCE_GXM_COLOR_FORMAT_A8B8G8R8
#define DISPLAY_COLOR_SURFACE_TYPE SCE_GXM_COLOR_SURFACE_LINEAR
#define DISPLAY_PIXEL_FORMAT SCE_DISPLAY_PIXELFORMAT_A8B8G8R8

#define GXM_PRINT_ERROR(status) sceClibPrintf("[line %d] failed with reason: %s\n", __LINE__, gxmnvg__easy_strerror(status))
#define GXM_CHECK_RETURN(func, ret)  \
    {                                \
        int status = func;           \
        if (status != SCE_OK)        \
        {                            \
            GXM_PRINT_ERROR(status); \
            return ret;              \
        }                            \
    }
#define GXM_CHECK(func) GXM_CHECK_RETURN(func, 0)
#define GXM_CHECK_VOID(func) GXM_CHECK_RETURN(func, )

struct NVGXMinitOptions {
    SceGxmMultisampleMode msaa;
    int swapInterval;
    int dumpShader; // dump shader to ux0:data/nvg_name_type.c
    int scenesPerFrame;
};
typedef struct NVGXMinitOptions NVGXMinitOptions;

struct NVGXMshaderProgram {
    SceGxmShaderPatcherId vert_id;
    SceGxmShaderPatcherId frag_id;

    SceGxmVertexProgram *vert;
    SceGxmFragmentProgram *frag;

    SceGxmProgram *vert_gxp;
    SceGxmProgram *frag_gxp;
};
typedef struct NVGXMshaderProgram NVGXMshaderProgram;

struct NVGXMtexture {
    SceGxmTexture tex;
    uint8_t *data;
    SceUID uid;
};
typedef struct NVGXMtexture NVGXMtexture;

struct NVGXMframebufferInitOptions {
    int display_buffer_count;
    int scenesPerFrame;

    /**
     * render_target is the framebuffer to render to.
     * NULL for default framebuffer
     */
    NVGXMtexture *render_target;
    SceGxmColorFormat color_format;
    SceGxmColorSurfaceType color_surface_type;
    int display_width;
    int display_height;
    int display_stride;
};
typedef struct NVGXMframebufferInitOptions NVGXMframebufferInitOptions;

struct NVGXMcolorSurface {
    SceGxmColorSurface surface;
    SceUID surface_uid;
    void *surface_addr;
    SceGxmSyncObject *sync_object;
};
typedef struct NVGXMcolorSurface NVGXMcolorSurface;

struct NVGXMframebuffer {
    SceGxmRenderTarget *gxm_render_target;

    NVGXMcolorSurface *gxm_color_surfaces;
    unsigned int gxm_front_buffer_index;
    unsigned int gxm_back_buffer_index;

    SceUID gxm_depth_stencil_surface_uid;
    void *gxm_depth_stencil_surface_addr;
    SceGxmDepthStencilSurface gxm_depth_stencil_surface;

    NVGXMframebufferInitOptions initOptions;
};
typedef struct NVGXMframebuffer NVGXMframebuffer;

struct NVGXMwindow {
    SceGxmContext *context;
    SceGxmShaderPatcher *shader_patcher;
    SceGxmMultisampleMode msaa;

    SceUID vdm_ring_buffer_uid;
    void *vdm_ring_buffer_addr;
    SceUID vertex_ring_buffer_uid;
    void *vertex_ring_buffer_addr;
    SceUID fragment_ring_buffer_uid;
    void *fragment_ring_buffer_addr;
    SceUID fragment_usse_ring_buffer_uid;
    void *fragment_usse_ring_buffer_addr;

    SceUID gxm_shader_patcher_buffer_uid;
    void *gxm_shader_patcher_buffer_addr;
    SceUID gxm_shader_patcher_vertex_usse_uid;
    void *gxm_shader_patcher_vertex_usse_addr;
    SceUID gxm_shader_patcher_fragment_usse_uid;
    void *gxm_shader_patcher_fragment_usse_addr;

    NVGXMframebuffer *fb;
};
typedef struct NVGXMwindow NVGXMwindow;

/**
 * Helper functions to create shader program.
 */
int gxmCreateFragmentProgram(SceGxmShaderPatcherId programId,
                             SceGxmOutputRegisterFormat outputFormat,
                             const SceGxmBlendInfo *blendInfo,
                             const SceGxmProgram *vertexProgram,
                             SceGxmFragmentProgram **fragmentProgram);

int gxmCreateVertexProgram(SceGxmShaderPatcherId programId,
                           const SceGxmVertexAttribute *attributes,
                           unsigned int attributeCount,
                           const SceGxmVertexStream *streams,
                           unsigned int streamCount,
                           SceGxmVertexProgram **vertexProgram);

NVGXMwindow *gxmCreateWindow(const NVGXMinitOptions *opts);

NVGXMwindow *gxmGetWindow(void);

void gxmDeleteWindow(NVGXMwindow *window);

NVGXMframebuffer *gxmCreateFramebuffer(const NVGXMframebufferInitOptions *opts);

void gxmDeleteFramebuffer(NVGXMframebuffer *fb);

NVGXMtexture *gxmCreateTexture(int width, int height, SceGxmTextureFormat format, void *data);

void gxmDeleteTexture(NVGXMtexture *texture);

/**
 * @brief Begin a scene.
 */
void gxmBeginFrame(void);

void gxmBeginFrameEx(NVGXMframebuffer *fb, unsigned int flags);

/**
 * @brief End a scene.
 */
void gxmEndFrame(void);

/**
 * @brief Swap the buffers.
 */
void gxmSwapBuffer(void);

/**
 * @brief Set the clear color.
 */
void gxmClearColor(float r, float g, float b, float a);

/**
 * @brief Clear the framebuffer and stencil buffer.
 * Must be called between gxmBeginFrame and gxmEndFrame.
 */
void gxmClear(void);

/**
 * @brief Set the scissor rectangle.
 */
void gxmScissor(int x, int y, int w, int h);

/**
 * @brief Get framebuffer data.
 */
void *gxmReadPixels(void);

/**
 * @brief Set the swap interval.
 * @param interval N for vsync, 0 for immediate.
 */
void gxmSwapInterval(int interval);

int gxmDialogUpdate(void);

unsigned short *gxmGetSharedIndices(void);

int gxmCreateShader(NVGXMshaderProgram *shader, const char *name, const char *vshader, const char *fshader);

void gxmDeleteShader(NVGXMshaderProgram *prog);

void gpu_unmap_free(SceUID uid);

void *gpu_alloc_map(SceKernelMemBlockType type, SceGxmMemoryAttribFlags gpu_attrib, size_t size, SceUID *uid);

#ifdef __cplusplus
}
#endif

#endif // NANOVG_GXM_UTILS_H

#ifdef NANOVG_GXM_UTILS_IMPLEMENTATION

#include <psp2/display.h>
#include <psp2/kernel/clib.h>
#include <psp2/common_dialog.h>

#include <stdlib.h>
#include <string.h>
#include <assert.h>

struct display_queue_callback_data {
    void *addr;
};

struct clear_vertex {
    float x, y;
};

static struct gxm_internal {
    SceGxmContext *context;
    SceGxmShaderPatcher *shader_patcher;
    NVGXMinitOptions initOptions;

    // clear shader
    NVGXMshaderProgram clearProg;
    NVGcolor clearColor;
    const SceGxmProgramParameter *clearParam;
    SceUID clearVerticesUid;
    struct clear_vertex *clearVertices;

    // shared indices
    SceUID linearIndicesUid;
    unsigned short *linearIndices;

    NVGXMwindow *window;
} gxm_internal;

static const char *gxmnvg__easy_strerror(int code) {
    switch ((SceGxmErrorCode) code) {
        case SCE_GXM_ERROR_UNINITIALIZED:
            return "SCE_GXM_ERROR_UNINITIALIZED";
        case SCE_GXM_ERROR_ALREADY_INITIALIZED:
            return "SCE_GXM_ERROR_ALREADY_INITIALIZED";
        case SCE_GXM_ERROR_OUT_OF_MEMORY:
            return "SCE_GXM_ERROR_OUT_OF_MEMORY";
        case SCE_GXM_ERROR_INVALID_VALUE:
            return "SCE_GXM_ERROR_INVALID_VALUE";
        case SCE_GXM_ERROR_INVALID_POINTER:
            return "SCE_GXM_ERROR_INVALID_POINTER";
        case SCE_GXM_ERROR_INVALID_ALIGNMENT:
            return "SCE_GXM_ERROR_INVALID_ALIGNMENT";
        case SCE_GXM_ERROR_NOT_WITHIN_SCENE:
            return "SCE_GXM_ERROR_NOT_WITHIN_SCENE";
        case SCE_GXM_ERROR_WITHIN_SCENE:
            return "SCE_GXM_ERROR_WITHIN_SCENE";
        case SCE_GXM_ERROR_NULL_PROGRAM:
            return "SCE_GXM_ERROR_NULL_PROGRAM";
        case SCE_GXM_ERROR_UNSUPPORTED:
            return "SCE_GXM_ERROR_UNSUPPORTED";
        case SCE_GXM_ERROR_PATCHER_INTERNAL:
            return "SCE_GXM_ERROR_PATCHER_INTERNAL";
        case SCE_GXM_ERROR_RESERVE_FAILED:
            return "SCE_GXM_ERROR_RESERVE_FAILED";
        case SCE_GXM_ERROR_PROGRAM_IN_USE:
            return "SCE_GXM_ERROR_PROGRAM_IN_USE";
        case SCE_GXM_ERROR_INVALID_INDEX_COUNT:
            return "SCE_GXM_ERROR_INVALID_INDEX_COUNT";
        case SCE_GXM_ERROR_INVALID_POLYGON_MODE:
            return "SCE_GXM_ERROR_INVALID_POLYGON_MODE";
        case SCE_GXM_ERROR_INVALID_SAMPLER_RESULT_TYPE_PRECISION:
            return "SCE_GXM_ERROR_INVALID_SAMPLER_RESULT_TYPE_PRECISION";
        case SCE_GXM_ERROR_INVALID_SAMPLER_RESULT_TYPE_COMPONENT_COUNT:
            return "SCE_GXM_ERROR_INVALID_SAMPLER_RESULT_TYPE_COMPONENT_COUNT";
        case SCE_GXM_ERROR_UNIFORM_BUFFER_NOT_RESERVED:
            return "SCE_GXM_ERROR_UNIFORM_BUFFER_NOT_RESERVED";
        case SCE_GXM_ERROR_INVALID_AUXILIARY_SURFACE:
            return "SCE_GXM_ERROR_INVALID_AUXILIARY_SURFACE";
        case SCE_GXM_ERROR_INVALID_PRECOMPUTED_DRAW:
            return "SCE_GXM_ERROR_INVALID_PRECOMPUTED_DRAW";
        case SCE_GXM_ERROR_INVALID_PRECOMPUTED_VERTEX_STATE:
            return "SCE_GXM_ERROR_INVALID_PRECOMPUTED_VERTEX_STATE";
        case SCE_GXM_ERROR_INVALID_PRECOMPUTED_FRAGMENT_STATE:
            return "SCE_GXM_ERROR_INVALID_PRECOMPUTED_FRAGMENT_STATE";
        case SCE_GXM_ERROR_DRIVER:
            return "SCE_GXM_ERROR_DRIVER";
        case SCE_GXM_ERROR_INVALID_TEXTURE:
            return "SCE_GXM_ERROR_INVALID_TEXTURE";
        case SCE_GXM_ERROR_INVALID_TEXTURE_DATA_POINTER:
            return "SCE_GXM_ERROR_INVALID_TEXTURE_DATA_POINTER";
        case SCE_GXM_ERROR_INVALID_TEXTURE_PALETTE_POINTER:
            return "SCE_GXM_ERROR_INVALID_TEXTURE_PALETTE_POINTER";
        case SCE_GXM_ERROR_OUT_OF_RENDER_TARGETS:
            return "SCE_GXM_ERROR_OUT_OF_RENDER_TARGETS";
        default:
            return "Unknown error";
    }
}

static void display_queue_callback(const void *callbackData) {
    SceDisplayFrameBuf display_fb;
    const struct display_queue_callback_data *cb_data = (struct display_queue_callback_data *) callbackData;

    memset(&display_fb, 0, sizeof(display_fb));
    display_fb.size = sizeof(display_fb);
    display_fb.base = cb_data->addr;
    display_fb.pitch = DISPLAY_STRIDE;
    display_fb.pixelformat = DISPLAY_PIXEL_FORMAT;
    display_fb.width = DISPLAY_WIDTH;
    display_fb.height = DISPLAY_HEIGHT;

    sceDisplaySetFrameBuf(&display_fb, SCE_DISPLAY_SETBUF_NEXTFRAME);

    if (gxm_internal.initOptions.swapInterval) {
        GXM_CHECK_VOID(sceDisplayWaitVblankStartMulti(gxm_internal.initOptions.swapInterval));
    }
}

void *gpu_alloc_map(SceKernelMemBlockType type, SceGxmMemoryAttribFlags gpu_attrib, size_t size, SceUID *uid) {
    SceUID memuid;
    void *addr;

    if (type == SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW)
        size = ALIGN(size, 256 * 1024);
    else
        size = ALIGN(size, 4 * 1024);

    memuid = sceKernelAllocMemBlock("gpumem", type, size, NULL);
    if (memuid < 0)
        return NULL;

    if (sceKernelGetMemBlockBase(memuid, &addr) < 0)
        return NULL;

    if (sceGxmMapMemory(addr, size, gpu_attrib) < 0) {
        sceKernelFreeMemBlock(memuid);
        return NULL;
    }

    if (uid)
        *uid = memuid;

    return addr;
}

void gpu_unmap_free(SceUID uid) {
    void *addr;

    if (uid == 0)
        return;

    if (sceKernelGetMemBlockBase(uid, &addr) < 0)
        return;

    sceGxmUnmapMemory(addr);

    sceKernelFreeMemBlock(uid);
}

static void *gpu_vertex_usse_alloc_map(size_t size, SceUID *uid, unsigned int *usse_offset) {
    SceUID memuid;
    void *addr;

    size = ALIGN(size, 4 * 1024);

    memuid = sceKernelAllocMemBlock("gpu_vertex_usse",
                                    SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE, size, NULL);
    if (memuid < 0)
        return NULL;

    if (sceKernelGetMemBlockBase(memuid, &addr) < 0)
        return NULL;

    if (sceGxmMapVertexUsseMemory(addr, size, usse_offset) < 0)
        return NULL;

    if (uid)
        *uid = memuid;

    return addr;
}

static void gpu_vertex_usse_unmap_free(SceUID uid) {
    void *addr;

    if (uid == 0)
        return;

    if (sceKernelGetMemBlockBase(uid, &addr) < 0)
        return;

    sceGxmUnmapVertexUsseMemory(addr);

    sceKernelFreeMemBlock(uid);
}

static void *gpu_fragment_usse_alloc_map(size_t size, SceUID *uid, unsigned int *usse_offset) {
    SceUID memuid;
    void *addr;

    size = ALIGN(size, 4 * 1024);

    memuid = sceKernelAllocMemBlock("gpu_fragment_usse",
                                    SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE, size, NULL);
    if (memuid < 0)
        return NULL;

    if (sceKernelGetMemBlockBase(memuid, &addr) < 0)
        return NULL;

    if (sceGxmMapFragmentUsseMemory(addr, size, usse_offset) < 0)
        return NULL;

    if (uid)
        *uid = memuid;

    return addr;
}

static void gpu_fragment_usse_unmap_free(SceUID uid) {
    void *addr;

    if (uid == 0)
        return;

    if (sceKernelGetMemBlockBase(uid, &addr) < 0)
        return;

    sceGxmUnmapFragmentUsseMemory(addr);

    sceKernelFreeMemBlock(uid);
}

static void *shader_patcher_host_alloc_cb(void *user_data, unsigned int size) {
    return malloc(size);
}

static void shader_patcher_host_free_cb(void *user_data, void *mem) {
    return free(mem);
}

NVGXMwindow *gxmCreateWindow(const NVGXMinitOptions *opts) {
    NVGXMwindow *window = NULL;

    /**
     * Alloc window
     */
    window = (NVGXMwindow *) malloc(sizeof(NVGXMwindow));
    if (window == NULL) {
        return NULL;
    }
    memset(window, 0, sizeof(NVGXMwindow));
    memcpy(&gxm_internal.initOptions, opts, sizeof(NVGXMinitOptions));
    window->msaa = opts->msaa;

    /**
     * Create gxm context
     */
    SceGxmInitializeParams gxm_init_params;
    memset(&gxm_init_params, 0, sizeof(gxm_init_params));
    gxm_init_params.flags = 0;
    gxm_init_params.displayQueueMaxPendingCount = MAX_PENDING_SWAPS;
    gxm_init_params.displayQueueCallback = display_queue_callback;
    gxm_init_params.displayQueueCallbackDataSize = sizeof(struct display_queue_callback_data);
    gxm_init_params.parameterBufferSize = SCE_GXM_DEFAULT_PARAMETER_BUFFER_SIZE;

    sceGxmInitialize(&gxm_init_params);

    window->vdm_ring_buffer_addr = gpu_alloc_map(SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW,
                                                 SCE_GXM_MEMORY_ATTRIB_READ,
                                                 SCE_GXM_DEFAULT_VDM_RING_BUFFER_SIZE,
                                                 &window->vdm_ring_buffer_uid);

    window->vertex_ring_buffer_addr = gpu_alloc_map(SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW,
                                                    SCE_GXM_MEMORY_ATTRIB_READ,
                                                    SCE_GXM_DEFAULT_VERTEX_RING_BUFFER_SIZE,
                                                    &window->vertex_ring_buffer_uid);

    window->fragment_ring_buffer_addr = gpu_alloc_map(SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW,
                                                      SCE_GXM_MEMORY_ATTRIB_READ,
                                                      SCE_GXM_DEFAULT_FRAGMENT_RING_BUFFER_SIZE,
                                                      &window->fragment_ring_buffer_uid);

    unsigned int fragment_usse_offset;
    window->fragment_usse_ring_buffer_addr = gpu_fragment_usse_alloc_map(
            SCE_GXM_DEFAULT_FRAGMENT_USSE_RING_BUFFER_SIZE,
            &window->fragment_usse_ring_buffer_uid, &fragment_usse_offset);

    SceGxmContextParams gxm_context_params;
    memset(&gxm_context_params, 0, sizeof(gxm_context_params));
    gxm_context_params.hostMem = malloc(SCE_GXM_MINIMUM_CONTEXT_HOST_MEM_SIZE);
    gxm_context_params.hostMemSize = SCE_GXM_MINIMUM_CONTEXT_HOST_MEM_SIZE;
    gxm_context_params.vdmRingBufferMem = window->vdm_ring_buffer_addr;
    gxm_context_params.vdmRingBufferMemSize = SCE_GXM_DEFAULT_VDM_RING_BUFFER_SIZE;
    gxm_context_params.vertexRingBufferMem = window->vertex_ring_buffer_addr;
    gxm_context_params.vertexRingBufferMemSize = SCE_GXM_DEFAULT_VERTEX_RING_BUFFER_SIZE;
    gxm_context_params.fragmentRingBufferMem = window->fragment_ring_buffer_addr;
    gxm_context_params.fragmentRingBufferMemSize = SCE_GXM_DEFAULT_FRAGMENT_RING_BUFFER_SIZE;
    gxm_context_params.fragmentUsseRingBufferMem = window->fragment_usse_ring_buffer_addr;
    gxm_context_params.fragmentUsseRingBufferMemSize = SCE_GXM_DEFAULT_FRAGMENT_USSE_RING_BUFFER_SIZE;
    gxm_context_params.fragmentUsseRingBufferOffset = fragment_usse_offset;

    sceGxmCreateContext(&gxm_context_params, &window->context);
    if (window->context == NULL) {
        gxmDeleteWindow(window);
        return NULL;
    }

    /**
     * Create default framebuffer
     */
    NVGXMframebufferInitOptions framebufferOpts = {
            .display_buffer_count = DISPLAY_BUFFER_COUNT,
            .scenesPerFrame = opts->scenesPerFrame,
            .render_target = NULL,
            .color_format = DISPLAY_COLOR_FORMAT,
            .color_surface_type = DISPLAY_COLOR_SURFACE_TYPE,
            .display_width = DISPLAY_WIDTH,
            .display_height = DISPLAY_HEIGHT,
            .display_stride = DISPLAY_STRIDE,
    };
    window->fb = gxmCreateFramebuffer(&framebufferOpts);
    if (!window->fb) {
        gxmDeleteWindow(window);
        return NULL;
    }

    /**
     * Create shader patcher
     */
    static const unsigned int shader_patcher_buffer_size = 64 * 1024;
    static const unsigned int shader_patcher_vertex_usse_size = 64 * 1024;
    static const unsigned int shader_patcher_fragment_usse_size = 64 * 1024;

    window->gxm_shader_patcher_buffer_addr = gpu_alloc_map(SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW,
                                                           SCE_GXM_MEMORY_ATTRIB_RW,
                                                           shader_patcher_buffer_size,
                                                           &window->gxm_shader_patcher_buffer_uid);

    unsigned int shader_patcher_vertex_usse_offset;
    window->gxm_shader_patcher_vertex_usse_addr = gpu_vertex_usse_alloc_map(
            shader_patcher_vertex_usse_size, &window->gxm_shader_patcher_vertex_usse_uid,
            &shader_patcher_vertex_usse_offset);

    unsigned int shader_patcher_fragment_usse_offset;
    window->gxm_shader_patcher_fragment_usse_addr = gpu_fragment_usse_alloc_map(
            shader_patcher_fragment_usse_size, &window->gxm_shader_patcher_fragment_usse_uid,
            &shader_patcher_fragment_usse_offset);

    SceGxmShaderPatcherParams shader_patcher_params;
    memset(&shader_patcher_params, 0, sizeof(shader_patcher_params));
    shader_patcher_params.userData = NULL;
    shader_patcher_params.hostAllocCallback = shader_patcher_host_alloc_cb;
    shader_patcher_params.hostFreeCallback = shader_patcher_host_free_cb;
    shader_patcher_params.bufferAllocCallback = NULL;
    shader_patcher_params.bufferFreeCallback = NULL;
    shader_patcher_params.bufferMem = window->gxm_shader_patcher_buffer_addr;
    shader_patcher_params.bufferMemSize = shader_patcher_buffer_size;
    shader_patcher_params.vertexUsseAllocCallback = NULL;
    shader_patcher_params.vertexUsseFreeCallback = NULL;
    shader_patcher_params.vertexUsseMem = window->gxm_shader_patcher_vertex_usse_addr;
    shader_patcher_params.vertexUsseMemSize = shader_patcher_vertex_usse_size;
    shader_patcher_params.vertexUsseOffset = shader_patcher_vertex_usse_offset;
    shader_patcher_params.fragmentUsseAllocCallback = NULL;
    shader_patcher_params.fragmentUsseFreeCallback = NULL;
    shader_patcher_params.fragmentUsseMem = window->gxm_shader_patcher_fragment_usse_addr;
    shader_patcher_params.fragmentUsseMemSize = shader_patcher_fragment_usse_size;
    shader_patcher_params.fragmentUsseOffset = shader_patcher_fragment_usse_offset;

    sceGxmShaderPatcherCreate(&shader_patcher_params, &window->shader_patcher);

    gxm_internal.context = window->context;
    gxm_internal.shader_patcher = window->shader_patcher;

    /**
     * Alloc shared linear indices
     */
    gxm_internal.linearIndices = (unsigned short *) gpu_alloc_map(
            SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
            SCE_GXM_MEMORY_ATTRIB_READ,
            UINT16_MAX * sizeof(unsigned short),
            &gxm_internal.linearIndicesUid);

    for (uint32_t i = 0; i < UINT16_MAX; ++i) {
        gxm_internal.linearIndices[i] = i;
    }

    /**
     * Create clear shader
     */
#if USE_VITA_SHARK
    static const char *clearVertShader = "float4 main(float2 position) : POSITION\n"
                                         "{\n"
                                         "	return float4(position, 1.f, 1.f);\n"
                                         "}\n";

    static const char *clearFragShader = "__nativecolor __regformat unsigned char4 main(uniform float4 color) : COLOR\n"
                                         "{\n"
                                         "	return unsigned char4(color * 255);\n"
                                         "}\n";
#else
    static const unsigned char clearVertShader[252] = {
        0x47, 0x58, 0x50, 0x00, 0x01, 0x05, 0x00, 0x03, 0xf9, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00,
        0x19, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0xb8, 0x00, 0x00, 0x00,
        0x70, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x04, 0x00, 0x00, 0x00, 0x80, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x74, 0x00, 0x00, 0x00, 0x70,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x00, 0x00, 0x00, 0x90, 0x3a,
        0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6c, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x5c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x54, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x4c, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x04,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07,
        0x44, 0xfa, 0x01, 0x00, 0x04, 0x90, 0x85, 0x11, 0xa5, 0x08, 0x01,
        0x80, 0x56, 0x90, 0x81, 0x11, 0x83, 0x08, 0x00, 0x00, 0x20, 0xa0,
        0x00, 0x50, 0x27, 0xfb, 0x10, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00,
        0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x70, 0x6f,
        0x73, 0x69, 0x74, 0x69, 0x6f, 0x6e, 0x00, 0x00, 0x00, 0x00
    };

    static const unsigned char clearFragShader[236] = {
        0x47, 0x58, 0x50, 0x00, 0x01, 0x05, 0x00, 0x03, 0xea, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x41, 0x20,
        0x18, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0xac, 0x00, 0x00, 0x00,
        0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x7c, 0x00,
        0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x68, 0x00, 0x00, 0x00, 0x6c,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x04, 0x00, 0x00, 0x00, 0x64, 0x00, 0x00, 0x00, 0x90, 0x3a,
        0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x58, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x58, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x48, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00,
        0x00, 0x01, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x07, 0x04, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,
        0x02, 0x80, 0x1d, 0xa0, 0x3e, 0x0c, 0x84, 0x40, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x44, 0xfa, 0x00, 0x00,
        0x00, 0xe0, 0x09, 0x00, 0x81, 0x50, 0x0e, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x04, 0x00, 0x10, 0x00, 0x00, 0x00, 0x01, 0xe4, 0x00, 0x00,
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x63, 0x6f, 0x6c,
        0x6f, 0x72, 0x00, 0x00, 0x00
    };
#endif
    if (gxmCreateShader(&gxm_internal.clearProg, "clear", (const char *) clearVertShader,
                        (const char *) clearFragShader) == 0) {
        gxmDeleteWindow(window);
        return NULL;
    }

    gxm_internal.clearVertices = (struct clear_vertex *) gpu_alloc_map(
            SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
            SCE_GXM_MEMORY_ATTRIB_READ,
            3 * sizeof(struct clear_vertex),
            &gxm_internal.clearVerticesUid);
    gxm_internal.clearVertices[0] = (struct clear_vertex) {-1.0f, -1.0f};
    gxm_internal.clearVertices[1] = (struct clear_vertex) {3.0f, -1.0f};
    gxm_internal.clearVertices[2] = (struct clear_vertex) {-1.0f, 3.0f};
    gxm_internal.clearParam = sceGxmProgramFindParameterByName(gxm_internal.clearProg.frag_gxp, "color");
    gxmClearColor(1.0f, 1.0f, 1.0f, 1.0f);

    const SceGxmProgramParameter *clear_position_param = sceGxmProgramFindParameterByName(
            gxm_internal.clearProg.vert_gxp,
            "position");
    SceGxmVertexAttribute clear_vertex_attribute;
    clear_vertex_attribute.streamIndex = 0;
    clear_vertex_attribute.offset = 0;
    clear_vertex_attribute.format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
    clear_vertex_attribute.componentCount = 2;
    clear_vertex_attribute.regIndex = sceGxmProgramParameterGetResourceIndex(clear_position_param);

    SceGxmVertexStream clear_vertex_stream;
    clear_vertex_stream.stride = sizeof(struct clear_vertex);
    clear_vertex_stream.indexSource = SCE_GXM_INDEX_SOURCE_INDEX_16BIT;

    GXM_CHECK(sceGxmShaderPatcherCreateVertexProgram(
            gxm_internal.shader_patcher, gxm_internal.clearProg.vert_id,
            &clear_vertex_attribute, 1,
            &clear_vertex_stream, 1,
            &gxm_internal.clearProg.vert));

    GXM_CHECK(sceGxmShaderPatcherCreateFragmentProgram(
            gxm_internal.shader_patcher, gxm_internal.clearProg.frag_id,
            SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4, SCE_GXM_MULTISAMPLE_NONE,
            NULL, gxm_internal.clearProg.vert_gxp,
            &gxm_internal.clearProg.frag));

    gxm_internal.window = window;
    return window;
}

void gxmDeleteWindow(NVGXMwindow *window) {
    if (window == NULL) return;

    gpu_unmap_free(gxm_internal.linearIndicesUid); // linear index buffer
    gpu_unmap_free(gxm_internal.clearVerticesUid); // clear vertex stream

    gxmDeleteShader(&gxm_internal.clearProg);

    if (window->shader_patcher)
        sceGxmShaderPatcherDestroy(window->shader_patcher);

    gpu_unmap_free(window->gxm_shader_patcher_buffer_uid);
    gpu_vertex_usse_unmap_free(window->gxm_shader_patcher_vertex_usse_uid);
    gpu_fragment_usse_unmap_free(window->gxm_shader_patcher_fragment_usse_uid);

    gxmDeleteFramebuffer(window->fb);

    gpu_unmap_free(window->vdm_ring_buffer_uid);
    gpu_unmap_free(window->vertex_ring_buffer_uid);
    gpu_unmap_free(window->fragment_ring_buffer_uid);
    gpu_fragment_usse_unmap_free(window->fragment_usse_ring_buffer_uid);

    if (window->context)
        sceGxmDestroyContext(window->context);
    sceGxmTerminate();

    free(window);
}

NVGXMwindow *gxmGetWindow(void) {
    return gxm_internal.window;
}

NVGXMframebuffer *gxmCreateFramebuffer(const NVGXMframebufferInitOptions *opts) {
    NVGXMframebuffer *fb = (NVGXMframebuffer *) malloc(sizeof(NVGXMframebuffer));
    if (fb == NULL) {
        return NULL;
    }
    assert(opts->scenesPerFrame >= 1 && opts->scenesPerFrame <= 8);
    assert(opts->display_buffer_count >= 1);
    assert(opts->render_target && opts->display_buffer_count == 1 || !opts->render_target);
    memset(fb, 0, sizeof(NVGXMframebuffer));
    memcpy(&fb->initOptions, opts, sizeof(NVGXMframebufferInitOptions));

    SceGxmRenderTargetParams render_target_params;
    memset(&render_target_params, 0, sizeof(render_target_params));
    render_target_params.flags = 0;
    render_target_params.width = opts->display_width;
    render_target_params.height = opts->display_height;
    render_target_params.scenesPerFrame = opts->scenesPerFrame;
    render_target_params.multisampleMode = gxm_internal.initOptions.msaa;
    render_target_params.multisampleLocations = 0;
    render_target_params.driverMemBlock = -1;

    sceGxmCreateRenderTarget(&render_target_params, &fb->gxm_render_target);

    fb->gxm_color_surfaces = (NVGXMcolorSurface *) malloc(opts->display_buffer_count * sizeof(NVGXMcolorSurface));
    if (fb->gxm_color_surfaces == NULL) {
        gxmDeleteFramebuffer(fb);
        return NULL;
    }

    memset(fb->gxm_color_surfaces, 0, opts->display_buffer_count * sizeof(NVGXMcolorSurface));
    for (int i = 0; i < opts->display_buffer_count; i++) {
        if (opts->render_target) {
            // Share the same data address between color surface and texture
            fb->gxm_color_surfaces[i].surface_addr = opts->render_target->data;
        } else {
            fb->gxm_color_surfaces[i].surface_addr = gpu_alloc_map(SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW,
                                                                   SCE_GXM_MEMORY_ATTRIB_RW,
                                                                   4 * opts->display_stride * opts->display_height,
                                                                   &fb->gxm_color_surfaces[i].surface_uid);
            if (fb->gxm_color_surfaces[i].surface_addr == NULL) {
                gxmDeleteFramebuffer(fb);
                return NULL;
            }
            sceGxmSyncObjectCreate(&fb->gxm_color_surfaces[i].sync_object);
        }

        memset(fb->gxm_color_surfaces[i].surface_addr, 0, 4 * opts->display_stride * opts->display_height);

        sceGxmColorSurfaceInit(&fb->gxm_color_surfaces[i].surface,
                               opts->color_format,
                               opts->color_surface_type,
                               (gxm_internal.initOptions.msaa == SCE_GXM_MULTISAMPLE_NONE)
                               ? SCE_GXM_COLOR_SURFACE_SCALE_NONE
                               : SCE_GXM_COLOR_SURFACE_SCALE_MSAA_DOWNSCALE,
                               SCE_GXM_OUTPUT_REGISTER_SIZE_32BIT,
                               opts->display_width,
                               opts->display_height,
                               opts->display_stride,
                               fb->gxm_color_surfaces[i].surface_addr);

    }

    unsigned int depth_stencil_width = ALIGN(opts->display_width, SCE_GXM_TILE_SIZEX);
    unsigned int depth_stencil_height = ALIGN(opts->display_height, SCE_GXM_TILE_SIZEY);
    unsigned int depth_stencil_samples = depth_stencil_width * depth_stencil_height;

    if (gxm_internal.initOptions.msaa == SCE_GXM_MULTISAMPLE_4X) {
        // samples increase in X and Y
        depth_stencil_samples *= 4;
        depth_stencil_width *= 2;
    } else if (gxm_internal.initOptions.msaa == SCE_GXM_MULTISAMPLE_2X) {
        // samples increase in Y only
        depth_stencil_samples *= 2;
    }
    fb->gxm_depth_stencil_surface_addr = gpu_alloc_map(SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW,
                                                       SCE_GXM_MEMORY_ATTRIB_RW,
                                                       4 * depth_stencil_samples, &fb->gxm_depth_stencil_surface_uid);
    if (fb->gxm_depth_stencil_surface_addr == NULL) {
        gxmDeleteFramebuffer(fb);
        return NULL;
    }

    sceGxmDepthStencilSurfaceInit(&fb->gxm_depth_stencil_surface,
                                  SCE_GXM_DEPTH_STENCIL_FORMAT_S8D24,
                                  SCE_GXM_DEPTH_STENCIL_SURFACE_TILED,
                                  depth_stencil_width,
                                  fb->gxm_depth_stencil_surface_addr,
                                  NULL);

    return fb;
}

void gxmDeleteFramebuffer(NVGXMframebuffer *fb) {
    if (fb == NULL)
        return;

    if (fb->gxm_depth_stencil_surface_uid)
        gpu_unmap_free(fb->gxm_depth_stencil_surface_uid);

    if (fb->gxm_color_surfaces) {
        for (int i = 0; i < fb->initOptions.display_buffer_count; i++) {
            if (fb->gxm_color_surfaces[i].surface_uid)
                gpu_unmap_free(fb->gxm_color_surfaces[i].surface_uid);
            if (fb->gxm_color_surfaces[i].sync_object)
                sceGxmSyncObjectDestroy(fb->gxm_color_surfaces[i].sync_object);
        }
        free(fb->gxm_color_surfaces);
    }

    sceGxmDestroyRenderTarget(fb->gxm_render_target);

    free(fb);
}

static int tex_format_to_bytespp(SceGxmTextureFormat format) {
    switch (format & 0x9f000000U) {
        case SCE_GXM_TEXTURE_BASE_FORMAT_U8:
        case SCE_GXM_TEXTURE_BASE_FORMAT_S8:
        case SCE_GXM_TEXTURE_BASE_FORMAT_P8:
            return 1;
        case SCE_GXM_TEXTURE_BASE_FORMAT_U4U4U4U4:
        case SCE_GXM_TEXTURE_BASE_FORMAT_U8U3U3U2:
        case SCE_GXM_TEXTURE_BASE_FORMAT_U1U5U5U5:
        case SCE_GXM_TEXTURE_BASE_FORMAT_U5U6U5:
        case SCE_GXM_TEXTURE_BASE_FORMAT_S5S5U6:
        case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8:
        case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8:
            return 2;
        case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8:
        case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8:
            return 3;
        case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8U8:
        case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8S8:
        case SCE_GXM_TEXTURE_BASE_FORMAT_F32:
        case SCE_GXM_TEXTURE_BASE_FORMAT_U32:
        case SCE_GXM_TEXTURE_BASE_FORMAT_S32:
        default:
            return 4;
    }
}

NVGXMtexture *gxmCreateTexture(int width, int height, SceGxmTextureFormat format, void *data) {
    int aligned_w = ALIGN(width, 8);
    int spp = tex_format_to_bytespp(format);
    int stride = aligned_w * spp;
    int tex_size = stride * height;
    int ret;

    NVGXMtexture *texture = (NVGXMtexture *) malloc(sizeof(NVGXMtexture));
    if (texture == NULL) {
        return NULL;
    }

    memset(texture, 0, sizeof(NVGXMtexture));
    texture->data = (uint8_t *) gpu_alloc_map(SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW,
                                              SCE_GXM_MEMORY_ATTRIB_RW,
                                              tex_size,
                                              &texture->uid);
    if (texture->data == NULL) {
        gxmDeleteTexture(texture);
        return NULL;
    }

    /* Clear the texture */
    if (data == NULL) {
        memset(texture->data, 0, tex_size);
    } else {
        for (int i = 0; i < height; i++) {
            memcpy(texture->data + i * stride, (uint8_t *) data + i * width * spp, width * spp);
        }
    }

    /* Create the gxm texture */
    ret = sceGxmTextureInitLinear(&texture->tex, texture->data, format, width, height, 0);
    if (ret < 0) {
        GXM_PRINT_ERROR(ret);
        gxmDeleteTexture(texture);
        return NULL;
    }

    return texture;
}

void gxmDeleteTexture(NVGXMtexture *texture) {
    if (texture == NULL) return;

    if (texture->uid)
        gpu_unmap_free(texture->uid);

    free(texture);
}

void gxmClearColor(float r, float g, float b, float a) {
    gxm_internal.clearColor.r = r;
    gxm_internal.clearColor.g = g;
    gxm_internal.clearColor.b = b;
    gxm_internal.clearColor.a = a;
}

void gxmClear(void) {
    sceGxmSetVertexProgram(gxm_internal.context, gxm_internal.clearProg.vert);
    sceGxmSetFragmentProgram(gxm_internal.context, gxm_internal.clearProg.frag);

    sceGxmSetVertexStream(gxm_internal.context, 0, gxm_internal.clearVertices);

    // set clear color
    void *buffer;
    sceGxmReserveFragmentDefaultUniformBuffer(gxm_internal.context, &buffer);
    sceGxmSetUniformDataF(buffer, gxm_internal.clearParam, 0, 4, gxm_internal.clearColor.rgba);

    // clear stencil buffer
    sceGxmSetFrontStencilRef(gxm_internal.context, 0);
    sceGxmSetBackStencilRef(gxm_internal.context, 0);
    sceGxmSetFrontStencilFunc(gxm_internal.context, SCE_GXM_STENCIL_FUNC_ALWAYS, SCE_GXM_STENCIL_OP_ZERO,
                              SCE_GXM_STENCIL_OP_ZERO, SCE_GXM_STENCIL_OP_ZERO, 0xff, 0xff);
    sceGxmSetBackStencilFunc(gxm_internal.context, SCE_GXM_STENCIL_FUNC_ALWAYS, SCE_GXM_STENCIL_OP_ZERO,
                             SCE_GXM_STENCIL_OP_ZERO, SCE_GXM_STENCIL_OP_ZERO, 0xff, 0xff);

    sceGxmDraw(gxm_internal.context,
               SCE_GXM_PRIMITIVE_TRIANGLES,
               SCE_GXM_INDEX_FORMAT_U16,
               gxm_internal.linearIndices,
               3);
}

void gxmScissor(int x, int y, int w, int h) {
    sceGxmSetRegionClip(gxm_internal.context, SCE_GXM_REGION_CLIP_OUTSIDE, x, y, x + w, y + h);
}

void gxmBeginFrame(void) {
    NVGXMwindow *window = gxm_internal.window;
    if (!window) return;
    NVGXMframebuffer *fb = window->fb;
    if (!fb) return;
    gxmBeginFrameEx(fb, 0);
}

void gxmBeginFrameEx(NVGXMframebuffer *fb, unsigned int flags) {
    GXM_CHECK_VOID(sceGxmBeginScene(gxm_internal.context,
                                    flags,
                                    fb->gxm_render_target,
                                    NULL,
                                    NULL,
                                    fb->gxm_color_surfaces[fb->gxm_back_buffer_index].sync_object,
                                    &fb->gxm_color_surfaces[fb->gxm_back_buffer_index].surface,
                                    &fb->gxm_depth_stencil_surface));
}

void gxmEndFrame(void) {
    GXM_CHECK_VOID(sceGxmEndScene(gxm_internal.context, NULL, NULL));
}

void gxmSwapBuffer(void) {
    NVGXMwindow *window = gxm_internal.window;
    if (!window) return;
    NVGXMframebuffer *fb = window->fb;
    if (!fb) return;
    struct display_queue_callback_data queue_cb_data;
    queue_cb_data.addr = fb->gxm_color_surfaces[fb->gxm_back_buffer_index].surface_addr;

    GXM_CHECK_VOID(sceGxmDisplayQueueAddEntry(
            fb->gxm_color_surfaces[fb->gxm_front_buffer_index].sync_object,
            fb->gxm_color_surfaces[fb->gxm_back_buffer_index].sync_object,
            &queue_cb_data));

    fb->gxm_front_buffer_index = fb->gxm_back_buffer_index;
    fb->gxm_back_buffer_index = (fb->gxm_back_buffer_index + 1) % fb->initOptions.display_buffer_count;
}

void gxmSwapInterval(int interval) {
    gxm_internal.initOptions.swapInterval = interval;
}

int gxmDialogUpdate(void) {
    NVGXMwindow *window = gxm_internal.window;
    if (!window) return SCE_COMMON_DIALOG_RESULT_ABORTED;
    NVGXMframebuffer *fb = window->fb;
    if (!fb) return SCE_COMMON_DIALOG_RESULT_ABORTED;
    SceCommonDialogUpdateParam updateParam;
    memset(&updateParam, 0, sizeof(updateParam));

    updateParam.renderTarget.colorFormat = fb->initOptions.color_format;
    updateParam.renderTarget.surfaceType = fb->initOptions.color_surface_type;
    updateParam.renderTarget.width = fb->initOptions.display_width;
    updateParam.renderTarget.height = fb->initOptions.display_height;
    updateParam.renderTarget.strideInPixels = fb->initOptions.display_stride;

    updateParam.renderTarget.colorSurfaceData = fb->gxm_color_surfaces[fb->gxm_back_buffer_index].surface_addr;
    updateParam.renderTarget.depthSurfaceData = fb->gxm_depth_stencil_surface_addr;
    updateParam.displaySyncObject = fb->gxm_color_surfaces[fb->gxm_back_buffer_index].sync_object;

    return sceCommonDialogUpdate(&updateParam);
}

void *gxmReadPixels(void) {
    NVGXMwindow *window = gxm_internal.window;
    if (!window) return NULL;
    NVGXMframebuffer *fb = window->fb;
    if (!fb) return NULL;
    return sceGxmColorSurfaceGetData(&fb->gxm_color_surfaces[fb->gxm_front_buffer_index].surface);
}

unsigned short *gxmGetSharedIndices(void) {
    return gxm_internal.linearIndices;
}

#ifdef USE_VITA_SHARK

void dumpShader(const char *name, const char *type, const SceGxmProgram *program, uint32_t size) {
    char path[256];

    int need_comma = 0;
    char *buf = (char *) malloc(0x5000);
    memset(buf, 0, 0x5000);
    memcpy(buf, program, size);

    snprintf(path, sizeof(path), "ux0:data/nvg_%s%s.c", name, type);
    FILE *fp = fopen(path, "w");
    if (fp) {
        fprintf(fp, "static const unsigned char %s%sShader[%i] = {", name, type, size);
        for (uint32_t i = 0; i < size; ++i) {
            if (need_comma)
                fprintf(fp, ", ");
            else
                need_comma = 1;
            if ((i % 11) == 0)
                fprintf(fp, "\n\t");
            fprintf(fp, "0x%.2x", buf[i] & 0xff);
        }
        fprintf(fp, "\n};\n\n");
        fclose(fp);
    }
}

int gxmCreateShader(NVGXMshaderProgram *shader, const char *name, const char *vshader, const char *fshader) {
    if (vshader != NULL) {
        uint32_t size = strlen(vshader);
        SceGxmProgram *p = shark_compile_shader_extended(vshader, &size, SHARK_VERTEX_SHADER, SHARK_OPT_FAST,
                                                         SHARK_ENABLE, SHARK_DISABLE, SHARK_ENABLE);
        if (!p) {
            sceClibPrintf("shark_compile_shader failed (vert): %s\n", name);
            shark_clear_output();
            return 0;
        }
        shader->vert_gxp = (SceGxmProgram *) malloc(size);
        sceClibMemcpy((void *) shader->vert_gxp, (void *) p, size);
        shark_clear_output();
        GXM_CHECK(sceGxmShaderPatcherRegisterProgram(gxm_internal.shader_patcher, shader->vert_gxp, &shader->vert_id));
        GXM_CHECK(sceGxmProgramCheck(shader->vert_gxp));

        if (gxm_internal.initOptions.dumpShader) {
            dumpShader(name, "Vert", shader->vert_gxp, size);
        }
    }

    if (fshader != NULL) {
        uint32_t size = strlen(fshader);
        SceGxmProgram *p = shark_compile_shader_extended(fshader, &size, SHARK_FRAGMENT_SHADER, SHARK_OPT_FAST,
                                                         SHARK_ENABLE, SHARK_DISABLE, SHARK_ENABLE);
        if (!p) {
            sceClibPrintf("shark_compile_shader failed (frag): %s\n", name);
            shark_clear_output();
            return 0;
        }
        shader->frag_gxp = (SceGxmProgram *) malloc(size);
        sceClibMemcpy((void *) shader->frag_gxp, (void *) p, size);
        shark_clear_output();
        GXM_CHECK(sceGxmShaderPatcherRegisterProgram(gxm_internal.shader_patcher, shader->frag_gxp, &shader->frag_id));
        GXM_CHECK(sceGxmProgramCheck(shader->frag_gxp));

        if (gxm_internal.initOptions.dumpShader) {
            dumpShader(name, "Frag", shader->frag_gxp, size);
        }
    }
    return 1;
}

#else

int gxmCreateShader(NVGXMshaderProgram *shader, const char *name, const char *vshader, const char *fshader) {
    (void) name;
    if (vshader != NULL) {
        shader->vert_gxp = (SceGxmProgram *) vshader;
        GXM_CHECK(sceGxmShaderPatcherRegisterProgram(gxm_internal.shader_patcher, shader->vert_gxp, &shader->vert_id));
        GXM_CHECK(sceGxmProgramCheck(shader->vert_gxp));
    }

    if (fshader != NULL) {
        shader->frag_gxp = (SceGxmProgram *) fshader;
        GXM_CHECK(sceGxmShaderPatcherRegisterProgram(gxm_internal.shader_patcher, shader->frag_gxp, &shader->frag_id));
        GXM_CHECK(sceGxmProgramCheck(shader->frag_gxp));
    }
    return 1;
}

#endif

void gxmDeleteShader(NVGXMshaderProgram *prog) {
    if (gxm_internal.shader_patcher == NULL)
        return;

    if (prog->vert)
        sceGxmShaderPatcherReleaseVertexProgram(gxm_internal.shader_patcher, prog->vert);
    if (prog->frag)
        sceGxmShaderPatcherReleaseFragmentProgram(gxm_internal.shader_patcher, prog->frag);

    if (prog->vert_id)
        sceGxmShaderPatcherUnregisterProgram(gxm_internal.shader_patcher, prog->vert_id);
    if (prog->frag_id)
        sceGxmShaderPatcherUnregisterProgram(gxm_internal.shader_patcher, prog->frag_id);

#ifdef USE_VITA_SHARK
    if (prog->vert_gxp)
        free(prog->vert_gxp);
    if (prog->frag_gxp)
        free(prog->frag_gxp);
#endif
}

int gxmCreateFragmentProgram(SceGxmShaderPatcherId programId,
                             SceGxmOutputRegisterFormat outputFormat,
                             const SceGxmBlendInfo *blendInfo,
                             const SceGxmProgram *vertexProgram,
                             SceGxmFragmentProgram **fragmentProgram) {
    return sceGxmShaderPatcherCreateFragmentProgram(gxm_internal.shader_patcher,
                                                    programId,
                                                    outputFormat,
                                                    gxm_internal.initOptions.msaa,
                                                    blendInfo,
                                                    vertexProgram,
                                                    fragmentProgram);
}

int gxmCreateVertexProgram(SceGxmShaderPatcherId programId,
                           const SceGxmVertexAttribute *attributes,
                           unsigned int attributeCount,
                           const SceGxmVertexStream *streams,
                           unsigned int streamCount,
                           SceGxmVertexProgram **vertexProgram) {
    return sceGxmShaderPatcherCreateVertexProgram(gxm_internal.shader_patcher,
                                                  programId,
                                                  attributes,
                                                  attributeCount,
                                                  streams,
                                                  streamCount,
                                                  vertexProgram);
}

#endif // NANOVG_GXM_UTILS_IMPLEMENTATION
