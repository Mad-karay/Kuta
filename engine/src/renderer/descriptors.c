// Descriptors — descriptor pool, layout, and set allocation and updates.
#include <vulkan/vulkan.h>
#include <math.h>
#include "renderer/descriptors.h"
#include "util/log.h"


void descriptor_layout_builder_add_binding(Arena *a, DescriptorLayoutBuilder *builder, uint32_t binding, VkDescriptorType type) {
  VkDescriptorSetLayoutBinding new_bind = {
    .binding = binding,
    .descriptorType = type,
    .descriptorCount = 1,
  };
  arena_da_append(a, builder, new_bind);
}

void descriptor_layout_builder_clear(DescriptorLayoutBuilder *builder) {
  builder->count = 0;
}

bool descriptor_layout_builder_build(Arena *a, DescriptorLayoutBuilder *builder, VkDevice device, VkShaderStageFlags shader_stages, VkDescriptorSetLayout *out_layout, void *pNext, VkDescriptorSetLayoutCreateFlags flags) {
  for (size_t i = 0; i < builder->count; i++) {
    builder->items[i].stageFlags |= shader_stages;
  }

  VkDescriptorSetLayoutCreateInfo layout_info = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .pNext = pNext,
    .flags = flags,
    .bindingCount = (uint32_t)builder->count,
    .pBindings = builder->items,
  };

  VkResult layout_res = vkCreateDescriptorSetLayout(device, &layout_info, NULL, out_layout);
  if (layout_res != VK_SUCCESS) {
    LOG_E("Failed to create descriptor set layout: %d", layout_res);
    return true;
  }

  (void)a;
  return false;
}


bool init_descriptor_allocator(Arena *a, DescriptorAllocator *allocator, VkDevice device, uint32_t max_sets, PoolSizeRatio *ratios, uint32_t ratio_count) {
  if (ratio_count == 0) {
    LOG_E("Failed to init descriptor allocator: no pool size ratios provided");
    return true;
  }

  VkDescriptorPoolSize *pool_sizes = arena_alloc(a, sizeof(VkDescriptorPoolSize) * ratio_count);
  if (!pool_sizes) {
    LOG_E("Failed to allocate memory for descriptor pool sizes");
    return true;
  }

  for (uint32_t i = 0; i < ratio_count; i++) {
    pool_sizes[i] = (VkDescriptorPoolSize){
      .type = ratios[i].type,
      .descriptorCount = (uint32_t)ceilf(ratios[i].ratio * (float)max_sets),
    };
  }

  VkDescriptorPoolCreateInfo pool_info = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
    .flags = 0,
    .maxSets = max_sets,
    .poolSizeCount = ratio_count,
    .pPoolSizes = pool_sizes,
  };

  VkResult pool_res = vkCreateDescriptorPool(device, &pool_info, NULL, &allocator->pool);
  if (pool_res != VK_SUCCESS) {
    LOG_E("Failed to create descriptor pool: %d", pool_res);
    return true;
  }

  return false;
}

bool descriptor_allocator_reset(DescriptorAllocator *allocator, VkDevice device) {
  VkResult reset_res = vkResetDescriptorPool(device, allocator->pool, 0);
  if (reset_res != VK_SUCCESS) {
    LOG_E("Failed to reset descriptor pool: %d", reset_res);
    return true;
  }
  return false;
}

bool descriptor_allocator_allocate(DescriptorAllocator *allocator, VkDevice device, VkDescriptorSetLayout layout, VkDescriptorSet *out_set) {
  VkDescriptorSetAllocateInfo alloc_info = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
    .pNext = NULL,
    .descriptorPool = allocator->pool,
    .descriptorSetCount = 1,
    .pSetLayouts = &layout,
  };

  VkResult alloc_res = vkAllocateDescriptorSets(device, &alloc_info, out_set);
  if (alloc_res != VK_SUCCESS) {
    LOG_E("Failed to allocate descriptor set: %d", alloc_res);
    return true;
  }

  return false;
}

void deinit_descriptor_allocator(DescriptorAllocator *allocator, VkDevice device) {
  if (allocator->pool != VK_NULL_HANDLE) {
    vkDestroyDescriptorPool(device, allocator->pool, NULL);
    allocator->pool = VK_NULL_HANDLE;
  }
}
