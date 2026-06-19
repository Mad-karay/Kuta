// Commands — immediate submit helper and reusable command recording utilities.
#include <vulkan/vulkan.h>
#include "renderer/device.h"
#include "renderer/commands.h"
#include "util/log.h"

bool init_immediate_commands(DeviceCtx *dev_ctx, ImmediateCtx *imm) {
  VkCommandPoolCreateInfo pool_info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    .queueFamilyIndex = dev_ctx->graphics_family,
  };

  VkResult pool_res = vkCreateCommandPool(dev_ctx->device, &pool_info, NULL, &imm->pool);
  if (pool_res != VK_SUCCESS) {
    LOG_E("Failed to create immediate command pool: %d", pool_res);
    return true;
  }

  VkCommandBufferAllocateInfo alloc_info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .commandPool = imm->pool,
    .commandBufferCount = 1,
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
  };

  VkResult alloc_res = vkAllocateCommandBuffers(dev_ctx->device, &alloc_info, &imm->cmd);
  if (alloc_res != VK_SUCCESS) {
    LOG_E("Failed to allocate immediate command buffer: %d", alloc_res);
    return true;
  }

  // Created signaled-free; you'll typically reset/wait on this fence around each immediate submit.
  VkFenceCreateInfo fence_info = {
    .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
  };
  VkResult fence_res = vkCreateFence(dev_ctx->device, &fence_info, NULL, &imm->fence);
  if (fence_res != VK_SUCCESS) {
    LOG_E("Failed to create immediate fence: %d", fence_res);
    return true;
  }

  return false;
}
