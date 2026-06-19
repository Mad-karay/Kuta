#pragma once
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

typedef struct {
    VkBuffer      handle;
    VmaAllocation alloc;
    VkDeviceSize  size;
} AllocatedBuffer;
