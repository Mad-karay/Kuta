#pragma once
#include "core/window.h"
#include "core/input.h"
#include "renderer/device.h"
#include "renderer/swapchain.h"
#include "renderer/frame.h"
#include "renderer/image.h"
#include "renderer/commands.h"
#include "scene/camera.h"

typedef struct {
    WindowCtx    window_ctx;
    InputState   input;
    DeviceCtx    device_ctx;
    SwapchainCtx swapchain_ctx;
    FrameCtx     frames[FRAMES_IN_FLIGHT];
    uint32_t     frame_index;
    AllocatedImage draw_image;
    AllocatedImage depth_image;
    ImmediateCtx   immediate_ctx;
    Camera         camera;
} KutaCtx;

bool init_engine(Arena *a, KutaCtx *ctx, char* engine_name, char* app_name, char* window_title, uint32_t window_width, uint32_t window_height, uint32_t api_version);

void deinit_engine(KutaCtx *ctx);
void main_loop(KutaCtx *ctx);
