#pragma once
#include <vulkan/vulkan.h>
#include "renderer/device.h"

#define FRAMES_IN_FLIGHT 2

typedef struct {
    VkCommandPool   cmd_pool;
    VkCommandBuffer cmd;
    VkSemaphore     acquire_semaphore;
    VkFence         fence;
} FrameCtx;

bool init_frame_commands(DeviceCtx *dev_ctx, FrameCtx *frames, uint32_t frame_count);
void deinit_frame_commands(DeviceCtx *dev_ctx, FrameCtx *frames, uint32_t frame_count);
