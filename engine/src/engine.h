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
#include "scene/mesh.h"

typedef struct KutaCtx{
    WindowCtx    window_ctx;
    InputState   input;
    DeviceCtx    device_ctx;
    SwapchainCtx swapchain_ctx;
    Arena        swapchain_arena;
    FrameCtx     frames[FRAMES_IN_FLIGHT];
    uint32_t     frame_index;
    AllocatedImage draw_image;
    AllocatedImage depth_image;
    VkExtent2D   draw_extent;
    float render_scale;
    ImmediateCtx   immediate_ctx;
    Camera         camera;

    DescriptorAllocatorGrowable           global_descriptor_allocator;
    VkDescriptorSet          draw_image_descriptors;
    VkDescriptorSetLayout    draw_image_descriptor_layout;

    GPUSceneData          scene_data;
    VkDescriptorSetLayout gpu_scene_data_descriptor_layout;

    Pipeline gradient_pipeline;

    Pipeline mesh_pipeline;

    MeshAsset *loaded_meshes;
    size_t loaded_mesh_count;

    AllocatedImage white_image;
    AllocatedImage black_image;
    AllocatedImage grey_image;
    AllocatedImage error_checkerboard_image;

    VkSampler default_sampler_linear;
    VkSampler default_sampler_nearest;

    VkDescriptorSetLayout single_image_descriptor_layout;
} KutaCtx;

bool init_engine(Arena *a, KutaCtx *ctx, char* engine_name, char* app_name, char* window_title, uint32_t window_width, uint32_t window_height, uint32_t api_version);

void deinit_engine(KutaCtx *ctx);

void main_loop(Arena *a, KutaCtx *ctx);
