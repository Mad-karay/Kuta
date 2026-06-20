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

bool immediate_submit(DeviceCtx *dev_ctx, ImmediateCtx *imm, ImmediateSubmitFn fn, void *user_data) {
  if (vkResetFences(dev_ctx->device, 1, &imm->fence) != VK_SUCCESS) {
    LOG_E("Failed to reset immediate fence");
    return true;
  }
  if (vkResetCommandBuffer(imm->cmd, 0) != VK_SUCCESS) {
    LOG_E("Failed to reset immediate command buffer");
    return true;
  }

  VkCommandBuffer cmd = imm->cmd;

  VkCommandBufferBeginInfo cmd_begin_info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };

  if (vkBeginCommandBuffer(cmd, &cmd_begin_info) != VK_SUCCESS) {
    LOG_E("Failed to begin immediate command buffer");
    return true;
  }

  fn(cmd, user_data);

  if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
    LOG_E("Failed to end immediate command buffer");
    return true;
  }

  VkCommandBufferSubmitInfo cmd_submit_info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
    .commandBuffer = cmd,
  };

  VkSubmitInfo2 submit = {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
    .commandBufferInfoCount = 1,
    .pCommandBufferInfos = &cmd_submit_info,
  };

  if (vkQueueSubmit2(dev_ctx->graphics_queue, 1, &submit, imm->fence) != VK_SUCCESS) {
    LOG_E("Failed to submit immediate command buffer");
    return true;
  }

  if (vkWaitForFences(dev_ctx->device, 1, &imm->fence, VK_TRUE, 9999999999) != VK_SUCCESS) {
    LOG_E("Failed waiting on immediate fence");
    return true;
  }

  return false;
}

void deinit_immediate_commands(DeviceCtx *dev_ctx, ImmediateCtx *imm) {
  if (imm->fence != VK_NULL_HANDLE) {
    vkDestroyFence(dev_ctx->device, imm->fence, NULL);
    imm->fence = VK_NULL_HANDLE;
  }

  if (imm->pool != VK_NULL_HANDLE) {
    vkDestroyCommandPool(dev_ctx->device, imm->pool, NULL);
    imm->pool = VK_NULL_HANDLE;
  }
}
