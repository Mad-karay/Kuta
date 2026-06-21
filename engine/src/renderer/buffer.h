#pragma once
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <stdbool.h>

typedef struct {
    VkBuffer      handle;
    VmaAllocation alloc;
    VmaAllocationInfo info;
    VkDeviceSize  size;
} AllocatedBuffer;


bool create_buffer(VmaAllocator *allocator, size_t alloc_size, VkBufferUsageFlags usage, VmaMemoryUsage memory_usage, AllocatedBuffer *out_buffer);

void destroy_buffer(VmaAllocator *allocator, AllocatedBuffer *buffer);
