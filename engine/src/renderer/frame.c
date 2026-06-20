#include "renderer/frame.h"
#include <stdint.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include "renderer/device.h"
#include "util/arena.h"
#include "util/log.h"
#include "renderer/frame.h"


bool init_frame_commands(DeviceCtx *dev_ctx, FrameCtx *frames, uint32_t frame_count) {
  VkCommandPoolCreateInfo pool_info = { .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    .queueFamilyIndex = dev_ctx->graphics_family,
  };


  VkFenceCreateInfo fence_create_info = {
    .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    .pNext = NULL,
    .flags = VK_FENCE_CREATE_SIGNALED_BIT,
  };

  VkSemaphoreCreateInfo semaphore_create_info = {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    .pNext = NULL,
  };

  for (uint32_t i = 0; i < frame_count; i++) {
    VkResult pool_res = vkCreateCommandPool(dev_ctx->device, &pool_info, NULL, &frames[i].cmd_pool);
    if (pool_res != VK_SUCCESS) {
      LOG_E("Failed to create command pool for frame %u: %d", i, pool_res);
      return true;
    }

    VkCommandBufferAllocateInfo alloc_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = frames[i].cmd_pool,
      .commandBufferCount = 1,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    };

    VkResult alloc_res = vkAllocateCommandBuffers(dev_ctx->device, &alloc_info, &frames[i].cmd);
    if (alloc_res != VK_SUCCESS) {
      LOG_E("Failed to allocate command buffer for frame %u: %d", i, alloc_res);
      return true;
    }

    VkResult fence_res = vkCreateFence(dev_ctx->device, &fence_create_info, NULL, &frames[i].fence);
    if (fence_res != VK_SUCCESS) {
      LOG_E("Failed to create frame fence: %d", fence_res);
      return true;
    }
    VkResult sem_res = vkCreateSemaphore(dev_ctx->device, &semaphore_create_info, NULL, &frames[i].acquire_semaphore);
    if (sem_res != VK_SUCCESS) {
      LOG_E("Failed to create swapchain semaphore: %d", sem_res);
      return true;
    }
  }
  return false;
}



void deinit_frame_commands(DeviceCtx *dev_ctx, FrameCtx *frames, uint32_t frame_count) {
  for (uint32_t i = 0; i < frame_count; i++) {
    if (frames[i].acquire_semaphore != VK_NULL_HANDLE) {
      vkDestroySemaphore(dev_ctx->device, frames[i].acquire_semaphore, NULL);
      frames[i].acquire_semaphore = VK_NULL_HANDLE;
    }
    if (frames[i].fence != VK_NULL_HANDLE) {
      vkDestroyFence(dev_ctx->device, frames[i].fence, NULL);
      frames[i].fence = VK_NULL_HANDLE;
    }
    if (frames[i].cmd_pool != VK_NULL_HANDLE) {
      vkDestroyCommandPool(dev_ctx->device, frames[i].cmd_pool, NULL);
      frames[i].cmd_pool = VK_NULL_HANDLE;
    }
  }
}
