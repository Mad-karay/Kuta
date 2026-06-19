#pragma once
#include <vulkan/vulkan.h>
#include "renderer/device.h"

// Wraps a one-shot command buffer submission (useful for transfers and image transitions).
typedef struct {
    VkCommandPool   pool;
    VkCommandBuffer cmd;
    VkFence         fence;
} ImmediateCtx;

bool init_immediate_commands(DeviceCtx *dev_ctx, ImmediateCtx *imm);
