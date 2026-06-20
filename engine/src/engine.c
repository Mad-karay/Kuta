#include <stdint.h>
#include <math.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include "renderer/descriptors.h"
#include "renderer/device.h"
#include "renderer/frame.h"
#include "renderer/swapchain.h"
#include "core/window.h"
#include "util/log.h"
#include "util/arena.h"
#include "renderer/image.h"
#include "engine.h"
#include "vk_mem_alloc.h"
#include "renderer/pipeline.h"

bool init_descriptors(Arena *a, KutaCtx *ctx) {
  PoolSizeRatio sizes[] = {
    { .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .ratio = 1.0f },
  };

  if (init_descriptor_allocator(a, &ctx->global_descriptor_allocator, ctx->device_ctx.device, 10, sizes, 1)) {
    LOG_E("Failed to init descriptor allocator");
    return true;
  }

  /*Make the descriptor set layout for our compute draw*/
  {
    DescriptorLayoutBuilder builder = {0};
    descriptor_layout_builder_add_binding(a, &builder, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    if (descriptor_layout_builder_build(a, &builder, ctx->device_ctx.device, VK_SHADER_STAGE_COMPUTE_BIT, &ctx->draw_image_descriptor_layout, NULL, 0)) {
      LOG_E("Failed to build draw image descriptor layout");
      return true;
    }
  }

  if(descriptor_allocator_allocate(&ctx->global_descriptor_allocator, ctx->device_ctx.device, ctx->draw_image_descriptor_layout, &ctx->draw_image_descriptors)) {
    LOG_E("Failled to allocate image descriptors");
    return true;
  }

  VkDescriptorImageInfo img_info = {
    .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    .imageView = ctx->draw_image.view,
  };

  VkWriteDescriptorSet draw_image_write = {
    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
    .pNext = NULL,
    .dstBinding = 0,
    .dstSet = ctx->draw_image_descriptors,
    .descriptorCount = 1,
    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
    .pImageInfo = &img_info,
  };

  vkUpdateDescriptorSets(ctx->device_ctx.device, 1, &draw_image_write, 0, NULL);

  return false;
}

bool init_engine(Arena *a, KutaCtx *ctx, char* engine_name, char* app_name, char* window_title, uint32_t window_width, uint32_t window_height, uint32_t api_version) {

    if (init_window_context(&ctx->window_ctx, window_width, window_height, window_title)) {
      LOG_E("Failed to initialize window context");
      return true;
    }
    if(init_vulkan_context(a, &ctx->device_ctx, ctx->window_ctx.handle, engine_name, app_name, api_version)){
      LOG_E("Failed to initialize vulkan context structure");
      return true;
    }
    if(init_swapchain_ctx(a, &ctx->swapchain_ctx, &ctx->device_ctx, &ctx->window_ctx, &ctx->draw_image)){
      LOG_E("Failed to create Swapchain");
      return true;
    }
    if(init_frame_commands(&ctx->device_ctx, ctx->frames, FRAMES_IN_FLIGHT)) {
      LOG_E("Failed to init frame commands");
      return true;
    }
    if(init_descriptors(a, ctx)) {
      LOG_E("Failed to init descriptors");
      return true;
    }
    if(init_pipelines(ctx)) {
      LOG_E("Failed to init descriptors");
      return true;
    }
    return false;
}

void deinit_engine(KutaCtx *ctx) {
  vkDeviceWaitIdle(ctx->device_ctx.device);
  deinit_frame_commands(&ctx->device_ctx, ctx->frames, FRAMES_IN_FLIGHT);
  /*Destroy gpu resources in queue style*/
  vkDestroyImageView(ctx->device_ctx.device, ctx->draw_image.view, NULL);
  vmaDestroyImage(ctx->device_ctx.vma, ctx->draw_image.handle, ctx->draw_image.alloc);
  deinit_descriptor_allocator(&ctx->global_descriptor_allocator, ctx->device_ctx.device);
  vkDestroyDescriptorSetLayout(ctx->device_ctx.device, ctx->draw_image_descriptor_layout, NULL);
  vkDestroyPipelineLayout(ctx->device_ctx.device, ctx->gradient_pipeline.layout, NULL);
  vkDestroyPipeline(ctx->device_ctx.device, ctx->gradient_pipeline.handle, NULL);
  vmaDestroyAllocator(ctx->device_ctx.vma);
  /*Gpu Resources deinit ends here*/
  deinit_swapchain(&ctx->device_ctx, &ctx->swapchain_ctx);
  deinit_vulkan_context(&ctx->device_ctx);
  SDL_DestroyWindow(ctx->window_ctx.handle);
  SDL_Quit();
}


void draw_background(KutaCtx *ctx, VkCommandBuffer cmd, uint64_t frame_number, uint32_t swapchain_img_idx) {
  float flash = fabsf(sinf(frame_number / 120.0f));
  VkClearColorValue clear_value = { .float32 = { 0.0f, 0.0f, flash, 1.0f } };

  VkImageSubresourceRange clear_range = {
    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
    .baseMipLevel = 0,
    .levelCount = VK_REMAINING_MIP_LEVELS,
    .baseArrayLayer = 0,
    .layerCount = VK_REMAINING_ARRAY_LAYERS,
  };

  vkCmdClearColorImage(cmd, ctx->draw_image.handle, VK_IMAGE_LAYOUT_GENERAL, &clear_value, 1, &clear_range);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, ctx->gradient_pipeline.handle);

  vkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, ctx->gradient_pipeline.layout, 0, 1, &ctx->draw_image_descriptors, 0, NULL);

  vkCmdDispatch(cmd, (uint32_t)ceil(ctx->draw_extent.width / 16.0), (uint32_t)ceil(ctx->draw_extent.height / 16.0), 1);
}

bool draw(KutaCtx *ctx, uint64_t frame_number) {
  FrameCtx *current_frame = &ctx->frames[frame_number % FRAMES_IN_FLIGHT];

  vkWaitForFences(ctx->device_ctx.device, 1, &current_frame->fence, VK_TRUE, 1000000000);
  vkResetFences(ctx->device_ctx.device, 1, &current_frame->fence);

  uint32_t swapchain_img_idx;
  VkResult img_idx_res = vkAcquireNextImageKHR(ctx->device_ctx.device, ctx->swapchain_ctx.handle, 1000000000, current_frame->acquire_semaphore, NULL, &swapchain_img_idx);
  if (img_idx_res != VK_SUCCESS) {
      LOG_E("Failed to acquire next swapchain image");
      return true;
  }

  VkCommandBuffer cmd = current_frame->cmd;
  if(vkResetCommandBuffer(cmd, 0) != VK_SUCCESS) {
    LOG_E("Failed to reset command buffer");
    return true;
  }

  VkCommandBufferBeginInfo cmd_begin_info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .pNext = NULL,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    .pInheritanceInfo = NULL,
  };

  ctx->draw_extent.width = ctx->draw_image.extent.width;
  ctx->draw_extent.height = ctx->draw_image.extent.height;

  if(vkBeginCommandBuffer(cmd, &cmd_begin_info) != VK_SUCCESS) {
    LOG_E("Failed ot begin command buffer");
    return true;
  } 
  
  transition_image(cmd, ctx->draw_image.handle, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

  draw_background(ctx, cmd, frame_number, swapchain_img_idx);

  transition_image(cmd, ctx->draw_image.handle, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  transition_image(cmd, ctx->swapchain_ctx.images[swapchain_img_idx], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  copy_image_to_image(cmd, ctx->draw_image.handle, ctx->swapchain_ctx.images[swapchain_img_idx], ctx->draw_extent, ctx->swapchain_ctx.extent);

  transition_image(cmd, ctx->swapchain_ctx.images[swapchain_img_idx], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

  if(vkEndCommandBuffer(cmd) != VK_SUCCESS) {
    LOG_E("Failed ot finalize command buffer");
    return true;
  }

  VkCommandBufferSubmitInfo cmd_submit_info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
    .pNext = NULL,
    .commandBuffer = cmd,
    .deviceMask = 0,
  };

  VkSemaphoreSubmitInfo wait_info = {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
    .pNext = NULL,
    .semaphore = current_frame->acquire_semaphore,
    .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
    .deviceIndex = 0,
    .value = 1,
  };

  VkSemaphoreSubmitInfo signal_info = {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
    .pNext = NULL,
    .semaphore = ctx->swapchain_ctx.render_semaphores[swapchain_img_idx],
    .stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
    .deviceIndex = 0,
    .value = 1,
  };

  VkSubmitInfo2 submit = {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
    .pNext = NULL,
    .waitSemaphoreInfoCount = 1,
    .pWaitSemaphoreInfos = &wait_info,
    .signalSemaphoreInfoCount = 1,
    .pSignalSemaphoreInfos = &signal_info,
    .commandBufferInfoCount = 1,
    .pCommandBufferInfos = &cmd_submit_info,
  };

  vkQueueSubmit2(ctx->device_ctx.graphics_queue, 1, &submit, current_frame->fence);

  VkPresentInfoKHR present_info = {
    .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
    .pNext = NULL,
    .swapchainCount = 1,
    .pSwapchains = &ctx->swapchain_ctx.handle,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = &ctx->swapchain_ctx.render_semaphores[swapchain_img_idx],
    .pImageIndices = &swapchain_img_idx,
  };

  vkQueuePresentKHR(ctx->device_ctx.graphics_queue, &present_info);

  frame_number++;
  return false;
}

void main_loop(KutaCtx *ctx) {
  uint64_t frame_number = 0;
  bool running = true;
  SDL_Event event;
  while (running) {
    while (SDL_PollEvent(&event)) {
          if (event.type == SDL_EVENT_QUIT) {
              running = false;
          }
    }
    draw(ctx, frame_number);
    frame_number++;
  }
}
