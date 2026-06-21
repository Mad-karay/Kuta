#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include <vk_mem_alloc.h>
#include <stdbool.h>
#include "renderer/buffer.h"
#include "util/log.h"

bool create_buffer(VmaAllocator *allocator, size_t alloc_size, VkBufferUsageFlags usage, VmaMemoryUsage memory_usage, AllocatedBuffer *out_buffer) {
  VkBufferCreateInfo buffer_info = {
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .pNext = NULL,
    .size = alloc_size,
    .usage = usage,
  };

  VmaAllocationCreateInfo vma_alloc_info = {
    .usage = memory_usage,
    .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT,
  };

  AllocatedBuffer new_buffer;
  VkResult vma_aloc_res = vmaCreateBuffer(*allocator, &buffer_info, &vma_alloc_info, &new_buffer.handle, &new_buffer.alloc,
    &new_buffer.info);
  if(vma_aloc_res != VK_SUCCESS) {
    LOG_E("Failed to allocate vma buffer: %d", vma_aloc_res);
    return true;
  }

  *out_buffer = new_buffer;
  return false;
}

void destroy_buffer(VmaAllocator *allocator, AllocatedBuffer *buffer) {
    vmaDestroyBuffer(*allocator, buffer->handle, buffer->alloc);
}
