#pragma once
#include <vulkan/vulkan.h>
#include <stdint.h>
#include "renderer_mod/device.h"
#include "renderer_mod/image.h"
#include "util_mod/arena.h"

typedef struct {
    VkSwapchainKHR  handle;
    VkFormat        format;
    VkExtent2D      extent;
    VkImage        *images;
    VkImageView    *views;      
    VkSemaphore    *render_semaphores;
    uint32_t        image_count;
} SwapchainCtx;

typedef struct VkRendererCtx VkRendererCtx;

bool init_swapchain_ctx(Arena *a, SwapchainCtx *swp_ctx, DeviceCtx *dev_ctx, KtPlatform *pl, AllocatedImage *draw_image, AllocatedImage *depth_image);

void deinit_swapchain(DeviceCtx *dev_ctx, SwapchainCtx *swp_ctx);

bool resize_swapchain(VkRendererCtx *ctx);
