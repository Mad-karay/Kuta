// Swapchain — creation, recreation on resize, and image view management.
#include <stdint.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include "SDL3/SDL_video.h"
#include "core/window.h"
#include "renderer/device.h"
#include "util/arena.h"
#include "util/log.h"
#include "renderer/swapchain.h"

uint32_t clamp(uint32_t value, uint32_t min, uint32_t max) {
 if (value < min) {
    return min;
  } else if (value > max) {
    return max;
  }
  return value;
}

bool init_swapchain_ctx(Arena *a, SwapchainCtx *swp_ctx, DeviceCtx *dev_ctx, WindowCtx *win_ctx) {
  /*Query the surface capabilities for the current physical device and surface*/
  VkSurfaceCapabilitiesKHR capabilities; 
  VkResult capa_res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(dev_ctx->gpu, dev_ctx->surface, &capabilities);
  if (capa_res != VK_SUCCESS) {
    LOG_E("Failed to get surface capabilities: %d", capa_res);
    return true;
  }

  /*Get Format*/ 
  uint32_t format_count;
  VkResult form_count_res = vkGetPhysicalDeviceSurfaceFormatsKHR(dev_ctx->gpu, dev_ctx->surface, &format_count, NULL);
  if (form_count_res != VK_SUCCESS) {
    LOG_E("Failed to get the number of formats: %d", form_count_res);
    return true;
  }

  VkSurfaceFormatKHR *formats = arena_alloc(a, sizeof(VkSurfaceFormatKHR) * format_count);
  if (!formats) {
    LOG_E("Failed to allocate memory for formats");
    return true;
  }

  VkResult form_res = vkGetPhysicalDeviceSurfaceFormatsKHR(dev_ctx->gpu, dev_ctx->surface, &format_count, formats);
  if (form_res != VK_SUCCESS) { 
    LOG_E("Failed to get formats: %d", form_res);
    return true;
  }
  VkSurfaceFormatKHR format = formats[0]; 
  uint32_t format_index = 0;
  for (uint32_t i = 0; i < format_count; i++) {
      if (formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR &&
          formats[i].format == VK_FORMAT_B8G8R8A8_SRGB) {
        format_index = i;
        break;
      }
  }
  format = formats[format_index];
  swp_ctx->format = format.format;
   /*Select Present Mode*/

  VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
  uint32_t present_mode_count;

  VkResult pres_count_res = vkGetPhysicalDeviceSurfacePresentModesKHR(dev_ctx->gpu, dev_ctx->surface, &present_mode_count, NULL);
  if(pres_count_res != VK_SUCCESS) {
    LOG_E("Failed to get present mode count: %d", pres_count_res);
    return true;
  }

  VkPresentModeKHR *present_modes = arena_alloc(a, sizeof(VkPresentModeKHR) * present_mode_count);
  if(!present_modes) {
    LOG_E("Failed to allocate memory for present modes");
    return true;
  }

  // Try to find MAILBOX present mode for lower latency if available
  uint32_t present_mode_index = UINT32_MAX;
  for (uint32_t i = 0; i < present_mode_count; i++) {
    VkPresentModeKHR present = present_modes[i];
    if (present == VK_PRESENT_MODE_MAILBOX_KHR) {
      present_mode_index = i;
      break;
    }
  }
  // Use MAILBOX if available, otherwise use FIFO
  if (present_mode_index != UINT32_MAX) {
    present_mode = present_modes[present_mode_index];
  }
  
  /*Choose Extent*/

  VkExtent2D extent;
  if (capabilities.currentExtent.width != UINT32_MAX) {
      extent = capabilities.currentExtent;
  } else {
    int w, h;
    SDL_GetWindowSizeInPixels(win_ctx->handle, &w, &h);
    extent.width = clamp(w, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    extent.height = clamp(h, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
  }

  swp_ctx->extent = extent;

  /*Create Swapchain*/
  VkResult swp_res = vkCreateSwapchainKHR(
             dev_ctx->device,
             &(VkSwapchainCreateInfoKHR){
                 .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
                 .surface = dev_ctx->surface,
                 .queueFamilyIndexCount = 1,
                 .pQueueFamilyIndices = &dev_ctx->graphics_family,
                 .clipped = true,
                 .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                 .imageArrayLayers = 1,
                 .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
                 .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                 .oldSwapchain = swp_ctx->handle,
                 .preTransform = capabilities.currentTransform,
                 .imageExtent = swp_ctx->extent,
                 .imageFormat = format.format,
                 .imageColorSpace = format.colorSpace,
                 .presentMode = present_mode,
                 .minImageCount = clamp(3, capabilities.minImageCount,
                                        capabilities.maxImageCount
                                            ? capabilities.maxImageCount
                                            : UINT32_MAX),
             },
             NULL, &swp_ctx->handle);
  if (swp_res != VK_SUCCESS) {
    LOG_E("Failed ot create Swapchain: %d", swp_res);
    return true;
  }
  
  /*Get swapchain images and create image views*/
  VkResult swp_img_count_res = vkGetSwapchainImagesKHR(dev_ctx->device, swp_ctx->handle, &swp_ctx->image_count, NULL);
  if(swp_img_count_res != VK_SUCCESS) {
    LOG_E("Failed to get count of swapchain images: %d", swp_img_count_res);
    return true;
  }

  swp_ctx->images = arena_alloc(a, sizeof(VkImage) * swp_ctx->image_count);
  if(!swp_ctx->images) {
    LOG_E("Failed to allocate swapchain images");
    return true;
  }

  VkResult swp_img_res = vkGetSwapchainImagesKHR(dev_ctx->device, swp_ctx->handle, &swp_ctx->image_count, swp_ctx->images);
  if(swp_img_res != VK_SUCCESS) {
    LOG_E("Failed to get swapchain images: %d", swp_img_res);
    return true;
  }
  
  swp_ctx->views = arena_alloc(a, sizeof(VkImageView) * swp_ctx->image_count);
  if(!swp_ctx->views) {
    LOG_E("Failed to allocate memory for swapchain image views");
  }

  for (uint32_t i = 0; i < swp_ctx->image_count; i++) {
    VkResult view_res = vkCreateImageView(
               dev_ctx->device,
               &(VkImageViewCreateInfo){
                   .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                   .format = format.format,
                   .image = swp_ctx->images[i],
                   .components = (VkComponentMapping){},
                   .subresourceRange =
                       (VkImageSubresourceRange){
                           .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                           .layerCount = 1,
                           .levelCount = 1,
                       },
                   .viewType = VK_IMAGE_VIEW_TYPE_2D,
               },
               NULL, &swp_ctx->views[i]);
    if (view_res != VK_SUCCESS) {
      LOG_E("Failed to create image view %d, error code: %d", i, view_res);
      return true;
    }
  }

  swp_ctx->render_semaphores = arena_alloc(a, sizeof(VkSemaphore) * swp_ctx->image_count);
  VkSemaphoreCreateInfo sem_info = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
  for (uint32_t i = 0; i < swp_ctx->image_count; i++) {
      VkResult sem_res = vkCreateSemaphore(dev_ctx->device, &sem_info, NULL, &swp_ctx->render_semaphores[i]);
      if (sem_res != VK_SUCCESS) {
          LOG_E("Failed to create render semaphore %u: %d", i, sem_res);
          return true;
      }
  }

  return false;
}

void deinit_swapchain(DeviceCtx *dev_ctx, SwapchainCtx *swp_ctx) {
  if (swp_ctx->views) {
    for (uint32_t i = 0; i < swp_ctx->image_count; i++) {
      vkDestroySemaphore(dev_ctx->device, swp_ctx->render_semaphores[i], NULL);
      vkDestroyImageView(dev_ctx->device, swp_ctx->views[i], NULL);
    }
    swp_ctx->views = NULL;
    swp_ctx->images = NULL;
  }

  if (swp_ctx->handle != VK_NULL_HANDLE) {
    vkDestroySwapchainKHR(dev_ctx->device, swp_ctx->handle, NULL);
    swp_ctx->handle = VK_NULL_HANDLE;
  }
}
