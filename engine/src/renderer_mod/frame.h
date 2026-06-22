#pragma once
#include <vulkan/vulkan.h>
#include "renderer_mod/device.h"
#include "renderer_mod/buffer.h"
#include "renderer_mod/descriptors.h"

#define FRAMES_IN_FLIGHT 2

typedef struct {
    VkCommandPool   cmd_pool;
    VkCommandBuffer cmd;
    VkSemaphore     acquire_semaphore;
    VkFence         fence;
    DescriptorAllocatorGrowable frame_descriptors;
    AllocatedBuffer scene_data_buffer;
    bool            has_scene_data_buffer;
} FrameCtx;

bool init_frame_commands(DeviceCtx *dev_ctx, FrameCtx *frames, uint32_t frame_count);
void deinit_frame_commands(DeviceCtx *dev_ctx, FrameCtx *frames, uint32_t frame_count);
