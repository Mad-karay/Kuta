#pragma once
#include <vulkan/vulkan.h>

// Wraps a one-shot command buffer submission (useful for transfers and image transitions).
typedef struct {
    VkCommandPool   pool;
    VkCommandBuffer cmd;
    VkFence         fence;
} ImmediateCtx;
