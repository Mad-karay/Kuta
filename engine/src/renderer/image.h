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

void transition_image(VkCommandBuffer cmd, VkImage image, VkImageLayout current_layout, VkImageLayout new_layout);
void copy_image_to_image(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D src_size, VkExtent2D dst_size);
