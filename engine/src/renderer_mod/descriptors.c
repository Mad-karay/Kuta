#include <vulkan/vulkan.h>
#include <math.h>
#include <string.h>
#include "renderer_mod/descriptors.h"
#include "util_mod/log.h"

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

static VkDescriptorPool create_pool(Arena *a, DescriptorAllocatorGrowable *allocator, VkDevice device, uint32_t set_count, PoolSizeRatio *pool_ratios, size_t pool_ratio_count) {
  VkDescriptorPoolSize *pool_sizes = arena_alloc(a, sizeof(VkDescriptorPoolSize) * pool_ratio_count);
  if (!pool_sizes) {
    LOG_E("Failed to allocate memory for descriptor pool sizes");
    return VK_NULL_HANDLE;
  }

  for (size_t i = 0; i < pool_ratio_count; i++) {
    pool_sizes[i] = (VkDescriptorPoolSize){
      .type = pool_ratios[i].type,
      .descriptorCount = (uint32_t)(pool_ratios[i].ratio * (float)set_count),
    };
  }

  VkDescriptorPoolCreateInfo pool_info = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
    .flags = 0,
    .maxSets = set_count,
    .poolSizeCount = (uint32_t)pool_ratio_count,
    .pPoolSizes = pool_sizes,
  };

  VkDescriptorPool new_pool;
  VkResult pool_res = vkCreateDescriptorPool(device, &pool_info, NULL, &new_pool);
  if (pool_res != VK_SUCCESS) {
    LOG_E("Failed to create descriptor pool: %d", pool_res);
    return VK_NULL_HANDLE;
  }

  (void)allocator;
  return new_pool;
}

static VkDescriptorPool get_pool(Arena *a, DescriptorAllocatorGrowable *allocator, VkDevice device) {
  VkDescriptorPool new_pool;

  if (allocator->ready_pools.count != 0) {
    new_pool = allocator->ready_pools.items[allocator->ready_pools.count - 1];
    allocator->ready_pools.count--;
  } else {
    new_pool = create_pool(a, allocator, device, allocator->sets_per_pool, allocator->ratios, allocator->ratio_count);

    allocator->sets_per_pool = (uint32_t)((float)allocator->sets_per_pool * 1.5f);
    if (allocator->sets_per_pool > 4092) {
      allocator->sets_per_pool = 4092;
    }
  }

  return new_pool;
}

bool descriptor_allocator_growable_init(Arena *a, DescriptorAllocatorGrowable *allocator, VkDevice device, uint32_t initial_sets, PoolSizeRatio *pool_ratios, size_t pool_ratio_count) {
  allocator->ratios = arena_alloc(a, sizeof(PoolSizeRatio) * pool_ratio_count);
  if (!allocator->ratios) {
    LOG_E("Failed to allocate memory for pool size ratios");
    return true;
  }
  memcpy(allocator->ratios, pool_ratios, sizeof(PoolSizeRatio) * pool_ratio_count);
  allocator->ratio_count = pool_ratio_count;
  allocator->ratio_capacity = pool_ratio_count;

  VkDescriptorPool new_pool = create_pool(a, allocator, device, initial_sets, allocator->ratios, allocator->ratio_count);
  if (new_pool == VK_NULL_HANDLE) {
    LOG_E("Failed to create initial descriptor pool");
    return true;
  }

  allocator->sets_per_pool = (uint32_t)((float)initial_sets * 1.5f);

  allocator->ready_pools = (DescriptorPoolArray){0};
  allocator->full_pools = (DescriptorPoolArray){0};
  arena_da_append(a, &allocator->ready_pools, new_pool);

  return false;
}

void descriptor_allocator_growable_clear_pools(Arena *a, DescriptorAllocatorGrowable *allocator, VkDevice device) {
  for (size_t i = 0; i < allocator->ready_pools.count; i++) {
    vkResetDescriptorPool(device, allocator->ready_pools.items[i], 0);
  }
  for (size_t i = 0; i < allocator->full_pools.count; i++) {
    vkResetDescriptorPool(device, allocator->full_pools.items[i], 0);
    arena_da_append(a, &allocator->ready_pools, allocator->full_pools.items[i]);
  }
  allocator->full_pools.count = 0;
}

void descriptor_allocator_growable_destroy_pools(DescriptorAllocatorGrowable *allocator, VkDevice device) {
  for (size_t i = 0; i < allocator->ready_pools.count; i++) {
    vkDestroyDescriptorPool(device, allocator->ready_pools.items[i], NULL);
  }
  allocator->ready_pools.count = 0;

  for (size_t i = 0; i < allocator->full_pools.count; i++) {
    vkDestroyDescriptorPool(device, allocator->full_pools.items[i], NULL);
  }
  allocator->full_pools.count = 0;
}

bool descriptor_allocator_growable_allocate(Arena *a, DescriptorAllocatorGrowable *allocator, VkDevice device, VkDescriptorSetLayout layout, void *pNext, VkDescriptorSet *out_set) {
  VkDescriptorPool pool_to_use = get_pool(a, allocator, device);
  if (pool_to_use == VK_NULL_HANDLE) {
    LOG_E("Failed to get a descriptor pool to allocate from");
    return true;
  }

  VkDescriptorSetAllocateInfo alloc_info = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
    .pNext = pNext,
    .descriptorPool = pool_to_use,
    .descriptorSetCount = 1,
    .pSetLayouts = &layout,
  };

  VkResult result = vkAllocateDescriptorSets(device, &alloc_info, out_set);

  if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL) {
    arena_da_append(a, &allocator->full_pools, pool_to_use);

    pool_to_use = get_pool(a, allocator, device);
    if (pool_to_use == VK_NULL_HANDLE) {
      LOG_E("Failed to get a fallback descriptor pool to allocate from");
      return true;
    }
    alloc_info.descriptorPool = pool_to_use;

    result = vkAllocateDescriptorSets(device, &alloc_info, out_set);
    if (result != VK_SUCCESS) {
      LOG_E("Failed to allocate descriptor set after retry: %d", result);
      return true;
    }
  } else if (result != VK_SUCCESS) {
    LOG_E("Failed to allocate descriptor set: %d", result);
    return true;
  }

  arena_da_append(a, &allocator->ready_pools, pool_to_use);
  return false;
}


void descriptor_writer_clear(DescriptorWriter *writer) {
  writer->image_info_count = 0;
  writer->buffer_info_count = 0;
  writer->write_count = 0;
}

bool descriptor_writer_write_image(DescriptorWriter *writer, int binding, VkImageView image, VkSampler sampler, VkImageLayout layout, VkDescriptorType type) {
  if (writer->image_info_count >= DESCRIPTOR_WRITER_MAX_WRITES || writer->write_count >= DESCRIPTOR_WRITER_MAX_WRITES) {
    LOG_E("DescriptorWriter: too many writes, increase DESCRIPTOR_WRITER_MAX_WRITES");
    return true;
  }

  VkDescriptorImageInfo *info = &writer->image_infos[writer->image_info_count++];
  *info = (VkDescriptorImageInfo){
    .sampler = sampler,
    .imageView = image,
    .imageLayout = layout,
  };

  VkWriteDescriptorSet *write = &writer->writes[writer->write_count++];
  *write = (VkWriteDescriptorSet){
    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
    .dstBinding = (uint32_t)binding,
    .dstSet = VK_NULL_HANDLE, 
    .descriptorCount = 1,
    .descriptorType = type,
    .pImageInfo = info,
  };

  return false;
}

bool descriptor_writer_write_buffer(DescriptorWriter *writer, int binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type) {
  if (writer->buffer_info_count >= DESCRIPTOR_WRITER_MAX_WRITES || writer->write_count >= DESCRIPTOR_WRITER_MAX_WRITES) {
    LOG_E("DescriptorWriter: too many writes, increase DESCRIPTOR_WRITER_MAX_WRITES");
    return true;
  }

  VkDescriptorBufferInfo *info = &writer->buffer_infos[writer->buffer_info_count++];
  *info = (VkDescriptorBufferInfo){
    .buffer = buffer,
    .offset = offset,
    .range = size,
  };

  VkWriteDescriptorSet *write = &writer->writes[writer->write_count++];
  *write = (VkWriteDescriptorSet){
    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
    .dstBinding = (uint32_t)binding,
    .dstSet = VK_NULL_HANDLE, 
    .descriptorCount = 1,
    .descriptorType = type,
    .pBufferInfo = info,
  };

  return false;
}

void descriptor_writer_update_set(DescriptorWriter *writer, VkDevice device, VkDescriptorSet set) {
  for (uint32_t i = 0; i < writer->write_count; i++) {
    writer->writes[i].dstSet = set;
  }
  vkUpdateDescriptorSets(device, writer->write_count, writer->writes, 0, NULL);
}
