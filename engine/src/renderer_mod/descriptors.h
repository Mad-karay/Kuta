#pragma once

#include <vulkan/vulkan.h>
#include <stdint.h>
#include <stdbool.h>
#include "util_mod/arena.h"

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
    VkDescriptorPool *items;
    size_t            count;
    size_t            capacity;
} DescriptorPoolArray;

typedef struct {
    PoolSizeRatio       *ratios;
    size_t               ratio_count;
    size_t               ratio_capacity;

    DescriptorPoolArray  full_pools;
    DescriptorPoolArray  ready_pools;

    uint32_t             sets_per_pool;
} DescriptorAllocatorGrowable;

bool descriptor_allocator_growable_init(Arena *a, DescriptorAllocatorGrowable *allocator, VkDevice device, uint32_t initial_sets, PoolSizeRatio *pool_ratios, size_t pool_ratio_count);
void descriptor_allocator_growable_clear_pools(Arena *a, DescriptorAllocatorGrowable *allocator, VkDevice device);
void descriptor_allocator_growable_destroy_pools(DescriptorAllocatorGrowable *allocator, VkDevice device);
bool descriptor_allocator_growable_allocate(Arena *a, DescriptorAllocatorGrowable *allocator, VkDevice device, VkDescriptorSetLayout layout, void *pNext, VkDescriptorSet *out_set);

#define DESCRIPTOR_WRITER_MAX_WRITES 32

typedef struct {
    VkDescriptorImageInfo  image_infos[DESCRIPTOR_WRITER_MAX_WRITES];
    VkDescriptorBufferInfo buffer_infos[DESCRIPTOR_WRITER_MAX_WRITES];
    VkWriteDescriptorSet   writes[DESCRIPTOR_WRITER_MAX_WRITES];
    uint32_t                image_info_count;
    uint32_t                buffer_info_count;
    uint32_t                write_count;
} DescriptorWriter;

void descriptor_writer_clear(DescriptorWriter *writer);
bool descriptor_writer_write_image(DescriptorWriter *writer, int binding, VkImageView image, VkSampler sampler, VkImageLayout layout, VkDescriptorType type);
bool descriptor_writer_write_buffer(DescriptorWriter *writer, int binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type);
void descriptor_writer_update_set(DescriptorWriter *writer, VkDevice device, VkDescriptorSet set);
