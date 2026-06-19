#pragma once
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

typedef struct {
    VkImage       handle;
    VkImageView   view;
    VmaAllocation alloc;
    VkFormat      format;
    VkExtent3D    extent;
} AllocatedImage;
