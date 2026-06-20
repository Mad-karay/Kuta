#pragma once
#include <vulkan/vulkan.h>
#include <stdint.h>
#include "renderer/device.h"
#include "core/window.h"
#include "renderer/image.h"
#include "util/arena.h"

typedef struct {
    VkSwapchainKHR  handle;
    VkFormat        format;
    VkExtent2D      extent;
    VkImage        *images;
    VkImageView    *views;      
    VkSemaphore    *render_semaphores;
    uint32_t        image_count;
} SwapchainCtx;

bool init_swapchain_ctx(Arena *a, SwapchainCtx *swp_ctx, DeviceCtx *dev_ctx, WindowCtx *win_ctx, AllocatedImage *draw_image);
void deinit_swapchain(DeviceCtx *dev_ctx, SwapchainCtx *swp_ctx);
