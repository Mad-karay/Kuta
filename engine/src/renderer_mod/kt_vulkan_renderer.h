#pragma once

#include <stdint.h>

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <cglm/cglm.h>

#include "renderer_mod/device.h"
#include "renderer_mod/swapchain.h"
#include "renderer_mod/frame.h"
#include "renderer_mod/buffer.h"
#include "renderer_mod/image.h"
#include "renderer_mod/pipeline.h"
#include "renderer_mod/commands.h"
#include "renderer_mod/descriptors.h"
#include "scene_mod/mesh.h"
#include "util_mod/log.h"
#include "util_mod/arena.h"
#include "util_mod/platform.h"

#include "kuta/kuta_renderer.h"
#include "kuta/kuta_platform.h"


typedef struct {
    mat4 view;
    mat4 proj;
    mat4 view_proj;
    vec4 ambient_color;
    vec4 sunlight_direction;
    vec4 sunlight_color;
} GPUSceneData;

typedef struct {
    mat4            world_matrix;          
    VkDeviceAddress vertex_buffer;
} GPUDrawPushConstants;


#define KT_MAX_DRAWS 4096

typedef struct {
    uint32_t mesh_id;
    uint32_t mesh_index;
    uint32_t texture_id;
    mat4     transform;
} DrawEntry;

#define KT_MAX_MESHES    256
#define KT_MAX_TEXTURES  256

typedef struct {
    MeshAsset *asset;   
    size_t     count;
} MeshSlot;

typedef struct {
    AllocatedImage image;
    bool           alive;
} TextureSlot;


typedef struct VkRendererCtx{
    DeviceCtx    device;
    SwapchainCtx swapchain;
    FrameCtx     frames[FRAMES_IN_FLIGHT];
    uint32_t     frame_index;
    uint64_t     frame_number;

    AllocatedImage draw_image;
    AllocatedImage depth_image;
    VkExtent2D     draw_extent;
    float          render_scale;

    ImmediateCtx   immediate;

    Arena perma_arena;
    Arena swapchain_arena;
    Arena frame_arena;

    DescriptorAllocatorGrowable global_descriptor_allocator;
    VkDescriptorSet             draw_image_descriptors;
    VkDescriptorSetLayout       draw_image_descriptor_layout;
    VkDescriptorSetLayout       scene_data_descriptor_layout;
    VkDescriptorSetLayout       single_image_descriptor_layout;

    Pipeline gradient_pipeline;
    Pipeline mesh_pipeline;

    AllocatedImage white_image;
    AllocatedImage black_image;
    AllocatedImage grey_image;
    AllocatedImage error_checkerboard_image;
    VkSampler      sampler_linear;
    VkSampler      sampler_nearest;

    mat4 cam_view;
    mat4 cam_proj;
    bool has_camera;

    DrawEntry draw_list[KT_MAX_DRAWS];
    uint32_t  draw_count;

    MeshSlot    meshes  [KT_MAX_MESHES];
    TextureSlot textures[KT_MAX_TEXTURES];

    KtPlatform *platform;

    bool resize_requested;
} VkRendererCtx;


