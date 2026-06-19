#pragma once
#include <vulkan/vulkan.h>
#include <stdint.h>

typedef struct {
    VkDescriptorPool      pool;
    VkDescriptorSetLayout layout;
    VkDescriptorSet      *sets;
    uint32_t              count;
} DescriptorCtx;
