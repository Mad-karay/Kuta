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
    WindowCtx    window;
    InputState   input;
    DeviceCtx    device;
    SwapchainCtx swapchain;
    FrameCtx     frames[FRAMES_IN_FLIGHT];
    uint32_t     frame_index;
    AllocatedImage draw_image;
    AllocatedImage depth_image;
    ImmediateCtx   immediate;
    Camera         camera;
} Engine;
