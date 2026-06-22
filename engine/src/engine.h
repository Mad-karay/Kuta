#pragma once
#include "platform_mod/window.h"
#include "platform_mod/input.h"
#include "renderer_mod/device.h"
#include "renderer_mod/pipeline.h"
#include "renderer_mod/swapchain.h"
#include "renderer_mod/frame.h"
#include "renderer_mod/image.h"
#include "renderer_mod/commands.h"
#include "renderer_mod/descriptors.h"
#include "scene_mod/camera.h"
#include "scene_mod/mesh.h"

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
    float render_scale;
    ImmediateCtx   immediate_ctx;
    Camera         camera;

    Arena        swapchain_arena;
    Arena        frame_arena;
    Arena        perma_arena;

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

