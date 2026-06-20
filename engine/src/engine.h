#pragma once
#include "core/window.h"
#include "core/input.h"
#include "renderer/device.h"
#include "renderer/pipeline.h"
#include "renderer/swapchain.h"
#include "renderer/frame.h"
#include "renderer/image.h"
#include "renderer/commands.h"
#include "renderer/descriptors.h"
#include "scene/camera.h"

typedef struct KutaCtx{
    WindowCtx    window_ctx;
    InputState   input;
    DeviceCtx    device_ctx;
    SwapchainCtx swapchain_ctx;
    FrameCtx     frames[FRAMES_IN_FLIGHT];
    uint32_t     frame_index;
    AllocatedImage draw_image;
    AllocatedImage depth_image;
    VkExtent2D   draw_extent;
    ImmediateCtx   immediate_ctx;
    Camera         camera;

    DescriptorAllocator           global_descriptor_allocator;
    VkDescriptorSet          draw_image_descriptors;
    VkDescriptorSetLayout    draw_image_descriptor_layout;

    Pipeline gradient_pipeline;
} KutaCtx;

bool init_engine(Arena *a, KutaCtx *ctx, char* engine_name, char* app_name, char* window_title, uint32_t window_width, uint32_t window_height, uint32_t api_version);

void deinit_engine(KutaCtx *ctx);

void main_loop(KutaCtx *ctx);
