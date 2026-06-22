#pragma once
#include <vulkan/vulkan.h>
#include "renderer_mod/device.h"

typedef struct {
    VkCommandPool   pool;
    VkCommandBuffer cmd;
    VkFence         fence;
} ImmediateCtx;

bool init_immediate_commands(DeviceCtx *dev_ctx, ImmediateCtx *imm);

typedef void (*ImmediateSubmitFn)(VkCommandBuffer cmd, void *user_data);

bool immediate_submit(DeviceCtx *dev_ctx, ImmediateCtx *imm, ImmediateSubmitFn fn, void *user_data);
void deinit_immediate_commands(DeviceCtx *dev_ctx, ImmediateCtx *imm);
