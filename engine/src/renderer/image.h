#pragma once
#include <vulkan/vulkan.h>
#include <stdbool.h>
#include <vk_mem_alloc.h>
#include "renderer/device.h"
#include "renderer/commands.h"

typedef struct {
    VkImage       handle;
    VkImageView   view;
    VmaAllocation alloc;
    VkFormat      format;
    VkExtent3D    extent;
} AllocatedImage;

void transition_image(VkCommandBuffer cmd, VkImage image, VkImageLayout current_layout, VkImageLayout new_layout);
void copy_image_to_image(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D src_size, VkExtent2D dst_size);

bool create_image(VmaAllocator allocator, VkDevice device, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped, AllocatedImage *out_img);

bool create_image_with_data(DeviceCtx *dev_ctx, ImmediateCtx *imm, void *data,
                             VkExtent3D size, VkFormat format, VkImageUsageFlags usage,
                             bool mipmapped, AllocatedImage *out_img);

void destroy_image(DeviceCtx *dev_ctx, AllocatedImage *img);
