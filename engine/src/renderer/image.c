#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

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
