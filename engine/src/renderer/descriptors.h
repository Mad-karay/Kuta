#pragma once

#include <vulkan/vulkan.h>
#include <stdint.h>
#include <stdbool.h>
#include "util/arena.h"

typedef struct {
    VkDescriptorSetLayoutBinding *items;
    size_t                        count;
    size_t                        capacity;
} DescriptorLayoutBuilder;

void descriptor_layout_builder_add_binding(Arena *a, DescriptorLayoutBuilder *builder, uint32_t binding, VkDescriptorType type);
void descriptor_layout_builder_clear(DescriptorLayoutBuilder *builder);
bool descriptor_layout_builder_build(Arena *a, DescriptorLayoutBuilder *builder, VkDevice device, VkShaderStageFlags shader_stages, VkDescriptorSetLayout *out_layout, void *pNext, VkDescriptorSetLayoutCreateFlags flags);

typedef struct {
    VkDescriptorType type;
    float            ratio;
} PoolSizeRatio;

typedef struct {
    VkDescriptorPool pool;
} DescriptorAllocator;

bool init_descriptor_allocator(Arena *a, DescriptorAllocator *allocator, VkDevice device, uint32_t max_sets, PoolSizeRatio *ratios, uint32_t ratio_count);
bool descriptor_allocator_reset(DescriptorAllocator *allocator, VkDevice device);
bool descriptor_allocator_allocate(DescriptorAllocator *allocator, VkDevice device, VkDescriptorSetLayout layout, VkDescriptorSet *out_set);
void deinit_descriptor_allocator(DescriptorAllocator *allocator, VkDevice device);

