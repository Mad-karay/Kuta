#pragma once
#include <vulkan/vulkan.h>
#include <stdint.h>

typedef struct {
    VkSwapchainKHR  handle;
    VkFormat        format;
    VkExtent2D      extent;
    VkImage        *images;     // owned by the swapchain, do not destroy
    VkImageView    *views;      // we own these
    uint32_t        image_count;
} SwapchainCtx;
