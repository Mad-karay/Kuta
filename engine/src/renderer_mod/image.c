#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include <math.h>
#include <stdbool.h>
#include "renderer_mod/image.h"
#include "renderer_mod/buffer.h"
#include "renderer_mod/commands.h"
#include "util_mod/log.h"
#include <string.h>

typedef struct {
    VkImage     image;
    VkBuffer    buffer;
    VkExtent3D  extent;
} ImageUploadArgs;

static void copy_buffer_to_image(VkCommandBuffer cmd, void *user_data) {
  ImageUploadArgs *args = (ImageUploadArgs*)user_data;

  transition_image(cmd, args->image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  VkBufferImageCopy copy_region = {
    .bufferOffset = 0,
    .bufferRowLength = 0,
    .bufferImageHeight = 0,
    .imageSubresource = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .mipLevel = 0,
      .baseArrayLayer = 0,
      .layerCount = 1,
    },
    .imageExtent = args->extent,
  };

  vkCmdCopyBufferToImage(cmd, args->buffer, args->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

  transition_image(cmd, args->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void transition_image(VkCommandBuffer cmd, VkImage image, VkImageLayout current_layout, VkImageLayout new_layout) {
  VkImageAspectFlags aspect_mask = (new_layout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
  VkImageSubresourceRange sub_image = {
      .aspectMask = aspect_mask,
      .baseMipLevel = 0,
      .levelCount = VK_REMAINING_MIP_LEVELS,
      .baseArrayLayer = 0,
      .layerCount = VK_REMAINING_ARRAY_LAYERS,
  };

  VkImageMemoryBarrier2 image_barrier = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
    .pNext = NULL,
    .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
    .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
    .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
    .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
    .oldLayout = current_layout,
    .newLayout = new_layout,
    .subresourceRange = sub_image,
    .image = image,
  };

  VkDependencyInfo dep_info = {
    .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
    .pNext = NULL,
    .imageMemoryBarrierCount = 1,
    .pImageMemoryBarriers = &image_barrier,
  };

  vkCmdPipelineBarrier2(cmd, &dep_info);
}

void copy_image_to_image(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D src_size, VkExtent2D dst_size) {
  VkImageBlit2 blit_region = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
    .pNext = NULL,
    .srcOffsets = {
      [0] = { .x = 0, .y = 0, .z = 0 },
      [1] = { .x = (int32_t)src_size.width, .y = (int32_t)src_size.height, .z = 1 },
    },
    .dstOffsets = {
      [0] = { .x = 0, .y = 0, .z = 0 },
      [1] = { .x = (int32_t)dst_size.width, .y = (int32_t)dst_size.height, .z = 1 },
    },
    .srcSubresource = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .mipLevel = 0,
      .baseArrayLayer = 0,
      .layerCount = 1,
    },
    .dstSubresource = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .mipLevel = 0,
      .baseArrayLayer = 0,
      .layerCount = 1,
    },
  };

  VkBlitImageInfo2 blit_info = {
    .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
    .pNext = NULL,
    .srcImage = source,
    .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    .dstImage = destination,
    .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    .filter = VK_FILTER_LINEAR,
    .regionCount = 1,
    .pRegions = &blit_region,
  };

  vkCmdBlitImage2(cmd, &blit_info);
}




bool create_image(VmaAllocator allocator, VkDevice device, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped, AllocatedImage *out_img) {
  AllocatedImage new_image = {
    .format = format,
    .extent = size,
  };

  VkImageCreateInfo img_info = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .pNext = NULL,
    .imageType = VK_IMAGE_TYPE_2D,
    .format = format,
    .extent = size,
    .mipLevels = 1,
    .arrayLayers = 1,
    .samples = VK_SAMPLE_COUNT_1_BIT,
    .tiling = VK_IMAGE_TILING_OPTIMAL,
    .usage = usage,
  };

  if (mipmapped) {
    uint32_t max_dim = size.width > size.height ? size.width : size.height;
    img_info.mipLevels = (uint32_t)log2((double)max_dim) + 1;
	}

  VmaAllocationCreateInfo allocinfo = {
    .usage = VMA_MEMORY_USAGE_GPU_ONLY,
    .requiredFlags = (VkMemoryPropertyFlags)VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
  };
  
  VkResult img_create_res = vmaCreateImage(allocator, &img_info, &allocinfo, &new_image.handle, &new_image.alloc, NULL);
  if(img_create_res != VK_SUCCESS) {
    LOG_E("Failed to create image: %d", img_create_res);
    return true;
  }

  VkImageAspectFlags aspect_flag = VK_IMAGE_ASPECT_COLOR_BIT;
  if (format == VK_FORMAT_D32_SFLOAT) {
    aspect_flag = VK_IMAGE_ASPECT_DEPTH_BIT;
  }

  VkImageViewCreateInfo view_info = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .pNext = NULL,
    .viewType = VK_IMAGE_VIEW_TYPE_2D,
    .image = new_image.handle,
    .format = format,
    .subresourceRange.baseMipLevel = 0,
    .subresourceRange.levelCount = img_info.mipLevels,
    .subresourceRange.baseArrayLayer = 0,
    .subresourceRange.layerCount = 1,
    .subresourceRange.aspectMask = aspect_flag,
  };

  VkResult img_view_res = vkCreateImageView(device, &view_info, NULL, &new_image.view);
  if(img_view_res != VK_SUCCESS) {
    LOG_E("failed to create image view: %d", img_view_res);
    return true;
  }

  *out_img = new_image;
  return false;
}

bool create_image_with_data(DeviceCtx *dev_ctx, ImmediateCtx *imm, void *data,
                             VkExtent3D size, VkFormat format, VkImageUsageFlags usage,
                             bool mipmapped, AllocatedImage *out_img) {
  size_t data_size = (size_t)size.depth * size.width * size.height * 4;

  AllocatedBuffer upload_buffer;
  if (create_buffer(&dev_ctx->vma, data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, &upload_buffer)) {
    LOG_E("Failed to create image upload staging buffer");
    return true;
  }

  VmaAllocationInfo alloc_info;
  vmaGetAllocationInfo(dev_ctx->vma, upload_buffer.alloc, &alloc_info);
  memcpy(alloc_info.pMappedData, data, data_size);

  AllocatedImage new_image;
  if (create_image(dev_ctx->vma, dev_ctx->device, size, format,
                    usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                    mipmapped, &new_image)) {
    LOG_E("Failed to create image for upload");
    destroy_buffer(&dev_ctx->vma, &upload_buffer);
    return true;
  }

  ImageUploadArgs upload_args = {
    .image = new_image.handle,
    .buffer = upload_buffer.handle,
    .extent = size,
  };

  if (immediate_submit(dev_ctx, imm, copy_buffer_to_image, &upload_args)) {
    LOG_E("Failed to submit image upload commands");
    destroy_buffer(&dev_ctx->vma, &upload_buffer);
    return true;
  }

  destroy_buffer(&dev_ctx->vma, &upload_buffer);

  *out_img = new_image;
  return false;
}

void destroy_image(DeviceCtx *dev_ctx, AllocatedImage *img) {
  if (img->view != VK_NULL_HANDLE) {
    vkDestroyImageView(dev_ctx->device, img->view, NULL);
    img->view = VK_NULL_HANDLE;
  }
  if (img->handle != VK_NULL_HANDLE) {
    vmaDestroyImage(dev_ctx->vma, img->handle, img->alloc);
    img->handle = VK_NULL_HANDLE;
  }
}

