#pragma once
#include <vulkan/vulkan.h>

#define FRAMES_IN_FLIGHT 2

typedef struct {
    VkCommandPool   cmd_pool;
    VkCommandBuffer cmd;
    VkSemaphore     acquire_semaphore;
    VkSemaphore     render_semaphore;
    VkFence         fence;
} FrameCtx;
