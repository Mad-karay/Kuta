#include <stdint.h>
#include <math.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include "cglm/cglm.h"
#include "cglm/types.h"
#include "renderer/descriptors.h"
#include "renderer/device.h"
#include "renderer/frame.h"
#include "renderer/swapchain.h"
#include "core/window.h"
#include "scene/mesh.h"
#include "util/log.h"
#include "util/arena.h"
#include "renderer/image.h"
#include "engine.h"
#include "vk_mem_alloc.h"
#include "renderer/pipeline.h"
#include "util/platform.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

bool init_descriptors(Arena *a, KutaCtx *ctx) {
  PoolSizeRatio sizes[] = {
    { .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .ratio = 1.0f },
  };

  if (descriptor_allocator_growable_init(a, &ctx->global_descriptor_allocator, ctx->device_ctx.device, 10, sizes, 1)) {
    LOG_E("Failed to init descriptor allocator");
    return true;
  }

  {
    DescriptorLayoutBuilder builder = {0};
    descriptor_layout_builder_add_binding(a, &builder, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    if (descriptor_layout_builder_build(a, &builder, ctx->device_ctx.device, VK_SHADER_STAGE_COMPUTE_BIT, &ctx->draw_image_descriptor_layout, NULL, 0)) {
      LOG_E("Failed to build draw image descriptor layout");
      return true;
    }
  }

  {
    DescriptorLayoutBuilder builder = {0};
    descriptor_layout_builder_add_binding(a, &builder, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    if (descriptor_layout_builder_build(a, &builder, ctx->device_ctx.device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, &ctx->gpu_scene_data_descriptor_layout, NULL, 0)) {
      LOG_E("Failed to build scene data descriptor layout");
      return true;
    }
  }

  {
    DescriptorLayoutBuilder builder = {0};
    descriptor_layout_builder_add_binding(a, &builder, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    if (descriptor_layout_builder_build(a, &builder, ctx->device_ctx.device, VK_SHADER_STAGE_FRAGMENT_BIT, &ctx->single_image_descriptor_layout, NULL, 0)) {
      LOG_E("Failed to build single image descriptor layout");
      return true;
    }
  }

  if (descriptor_allocator_growable_allocate(a, &ctx->global_descriptor_allocator, ctx->device_ctx.device, ctx->draw_image_descriptor_layout, NULL, &ctx->draw_image_descriptors)) {
    LOG_E("Failed to allocate image descriptors");
    return true;
  }

  DescriptorWriter writer = {0};
  descriptor_writer_write_image(&writer, 0, ctx->draw_image.view, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
  descriptor_writer_update_set(&writer, ctx->device_ctx.device, ctx->draw_image_descriptors);

  PoolSizeRatio frame_sizes[] = {
    { .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .ratio = 3.0f },
    { .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .ratio = 3.0f },
    { .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .ratio = 3.0f },
    { .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .ratio = 4.0f },
  };

  for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
    if (descriptor_allocator_growable_init(a, &ctx->frames[i].frame_descriptors, ctx->device_ctx.device, 1000, frame_sizes, 4)) {
      LOG_E("Failed to init frame descriptor allocator %u", i);
      return true;
    }
  }

  return false;
}

bool init_mesh_pipeline(Arena *a, KutaCtx *ctx) {
  VkShaderModule triangle_frag_shader;
  size_t size;
  if(load_shader_module(&ctx->device_ctx.device, "./shaders/tex_image.frag.spv", &size, &triangle_frag_shader)){
    LOG_E("Failed to load colored triangle frag shader");
    return true;
  }

  VkShaderModule triangle_vert_shader;
  size_t size1;
  if(load_shader_module(&ctx->device_ctx.device, "./shaders/colored_triangle_mesh.vert.spv", &size1, &triangle_vert_shader)){
    LOG_E("Failed to load colored triangle vert shader");
    return true;
  }

  VkPushConstantRange buffer_range = {
    .offset = 0,
    .size = sizeof(GPUDrawPushConstants),
    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
  };

  VkPipelineLayoutCreateInfo pipeline_layout_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .flags = 0,
    .setLayoutCount = 1,
    .pSetLayouts = &ctx->single_image_descriptor_layout,
    .pushConstantRangeCount = 1,
    .pPushConstantRanges = &buffer_range,
  };

  VkResult mesh_pip_res = vkCreatePipelineLayout(ctx->device_ctx.device, &pipeline_layout_info, NULL, &ctx->mesh_pipeline.layout);
  if (mesh_pip_res != VK_SUCCESS) {
    LOG_E("Failed to create colored triangle pipeline layout: %d", mesh_pip_res);
    return true;
  }

  PipelineBuilder pb = {0};
  init_pipeline_builder(&pb);

  pb.pipeline_layout = ctx->mesh_pipeline.layout;
  pipeline_builder_set_shaders(a, &pb, triangle_vert_shader, triangle_frag_shader);
  pipeline_builder_set_input_topology(&pb, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
  pipeline_builder_set_polygon_mode(&pb, VK_POLYGON_MODE_FILL);
  pipeline_builder_set_cull_mode(&pb, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
  pipeline_builder_set_multisampling_none(&pb);
  pipeline_builder_disable_blending(&pb);
  // pipeline_builder_enable_blending_additive(&pb);
  pipeline_builder_enable_depthtest(&pb, true, VK_COMPARE_OP_GREATER_OR_EQUAL);

  pipeline_builder_set_color_attachment_format(&pb, ctx->draw_image.format);
  pipeline_builder_set_depth_format(&pb, ctx->depth_image.format);

  ctx->mesh_pipeline.handle = build_pipeline(a, &ctx->device_ctx.device, &pb);

  vkDestroyShaderModule(ctx->device_ctx.device, triangle_frag_shader, NULL);
	vkDestroyShaderModule(ctx->device_ctx.device, triangle_vert_shader, NULL);

  return false;
}

static uint32_t pack_unorm4x8(float r, float g, float b, float a) {
  uint8_t r8 = (uint8_t)roundf(fmaxf(0.0f, fminf(1.0f, r)) * 255.0f);
  uint8_t g8 = (uint8_t)roundf(fmaxf(0.0f, fminf(1.0f, g)) * 255.0f);
  uint8_t b8 = (uint8_t)roundf(fmaxf(0.0f, fminf(1.0f, b)) * 255.0f);
  uint8_t a8 = (uint8_t)roundf(fmaxf(0.0f, fminf(1.0f, a)) * 255.0f);
  return ((uint32_t)a8 << 24) | ((uint32_t)b8 << 16) | ((uint32_t)g8 << 8) | (uint32_t)r8;
}

bool init_default_data(Arena *a, KutaCtx *ctx) {
  uint32_t white = pack_unorm4x8(1.0f, 1.0f, 1.0f, 1.0f);
  if (create_image_with_data(&ctx->device_ctx, &ctx->immediate_ctx, &white,
                              (VkExtent3D){1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM,
                              VK_IMAGE_USAGE_SAMPLED_BIT, false, &ctx->white_image)) {
    LOG_E("Failed to create white default texture");
    return true;
  }

  uint32_t grey = pack_unorm4x8(0.66f, 0.66f, 0.66f, 1.0f);
  if (create_image_with_data(&ctx->device_ctx, &ctx->immediate_ctx, &grey,
                              (VkExtent3D){1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM,
                              VK_IMAGE_USAGE_SAMPLED_BIT, false, &ctx->grey_image)) {
    LOG_E("Failed to create grey default texture");
    return true;
  }

  uint32_t black = pack_unorm4x8(0.0f, 0.0f, 0.0f, 0.0f);
  if (create_image_with_data(&ctx->device_ctx, &ctx->immediate_ctx, &black,
                              (VkExtent3D){1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM,
                              VK_IMAGE_USAGE_SAMPLED_BIT, false, &ctx->black_image)) {
    LOG_E("Failed to create black default texture");
    return true;
  }

  uint32_t magenta = pack_unorm4x8(1.0f, 0.0f, 1.0f, 1.0f);
  uint32_t pixels[16 * 16];
  for (int x = 0; x < 16; x++) {
    for (int y = 0; y < 16; y++) {
      pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
    }
  }
  if (create_image_with_data(&ctx->device_ctx, &ctx->immediate_ctx, pixels,
                              (VkExtent3D){16, 16, 1}, VK_FORMAT_R8G8B8A8_UNORM,
                              VK_IMAGE_USAGE_SAMPLED_BIT, false, &ctx->error_checkerboard_image)) {
    LOG_E("Failed to create checkerboard default texture");
    return true;
  }

  VkSamplerCreateInfo sampl = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };

  sampl.magFilter = VK_FILTER_NEAREST;
  sampl.minFilter = VK_FILTER_NEAREST;
  if (vkCreateSampler(ctx->device_ctx.device, &sampl, NULL, &ctx->default_sampler_nearest) != VK_SUCCESS) {
    LOG_E("Failed to create nearest sampler");
    return true;
  }

  sampl.magFilter = VK_FILTER_LINEAR;
  sampl.minFilter = VK_FILTER_LINEAR;
  if (vkCreateSampler(ctx->device_ctx.device, &sampl, NULL, &ctx->default_sampler_linear) != VK_SUCCESS) {
    LOG_E("Failed to create linear sampler");
    return true;
  }
  
  if(load_gltf_meshes(a, &ctx->device_ctx, &ctx->immediate_ctx, "./assets/basicmesh.glb", &ctx->loaded_meshes, &ctx->loaded_mesh_count, true)) {
    LOG_E("Failed to load basicmesh glb file");
    return true;
  }
  return false;
}

bool init_engine(Arena *a, KutaCtx *ctx, char* engine_name, char* app_name, char* window_title, uint32_t window_width, uint32_t window_height, uint32_t api_version) {

    ctx->render_scale = 1.0f;
    if (init_window_context(&ctx->window_ctx, window_width, window_height, window_title)) {
      LOG_E("Failed to initialize window context");
      return true;
    }
    if(init_vulkan_context(a, &ctx->device_ctx, ctx->window_ctx.handle, engine_name, app_name, api_version)){
      LOG_E("Failed to initialize vulkan context structure");
      return true;
    }
    ctx->swapchain_arena = (Arena){0};
    if(init_swapchain_ctx(&ctx->swapchain_arena, &ctx->swapchain_ctx, &ctx->device_ctx, &ctx->window_ctx, &ctx->draw_image, &ctx->depth_image)){
      LOG_E("Failed to create Swapchain");
      return true;
    }
    if(init_frame_commands(&ctx->device_ctx, ctx->frames, FRAMES_IN_FLIGHT)) {
      LOG_E("Failed to init frame commands");
      return true;
    }
    if(init_descriptors(a, ctx)) {
      LOG_E("Failed to init descriptors");
      return true;
    }
    if(init_pipelines(ctx->device_ctx.device, &ctx->draw_image_descriptor_layout, &ctx->gradient_pipeline)) {
      LOG_E("Failed to init descriptors");
      return true;
    }
    if(init_mesh_pipeline(a, ctx)){
      LOG_E("Failed to init triangle pipeline");
      return true;
    }
    if(init_immediate_commands(&ctx->device_ctx, &ctx->immediate_ctx)) {
      LOG_E("Failed to init immediate commands");
      return true;
    }
    if(init_default_data(a, ctx)) {
      LOG_E("Failed to init default mesh data");
      return true;
    }

    return false;
}

void deinit_engine(KutaCtx *ctx) {
  vkDeviceWaitIdle(ctx->device_ctx.device);
  deinit_immediate_commands(&ctx->device_ctx, &ctx->immediate_ctx);
  deinit_frame_commands(&ctx->device_ctx, ctx->frames, FRAMES_IN_FLIGHT);
  /*Destroy gpu resources in queue style*/
  vkDestroyImageView(ctx->device_ctx.device, ctx->draw_image.view, NULL);
  vmaDestroyImage(ctx->device_ctx.vma, ctx->draw_image.handle, ctx->draw_image.alloc);
  vkDestroyImageView(ctx->device_ctx.device, ctx->depth_image.view, NULL);
  vmaDestroyImage(ctx->device_ctx.vma, ctx->depth_image.handle, ctx->depth_image.alloc);
  descriptor_allocator_growable_destroy_pools(&ctx->global_descriptor_allocator, ctx->device_ctx.device);
  for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
    descriptor_allocator_growable_destroy_pools(&ctx->frames[i].frame_descriptors, ctx->device_ctx.device);
  }
  vkDestroyDescriptorSetLayout(ctx->device_ctx.device, ctx->draw_image_descriptor_layout, NULL);
  vkDestroyDescriptorSetLayout(ctx->device_ctx.device, ctx->gpu_scene_data_descriptor_layout, NULL);
  vkDestroyDescriptorSetLayout(ctx->device_ctx.device, ctx->single_image_descriptor_layout, NULL);
  vkDestroyPipelineLayout(ctx->device_ctx.device, ctx->gradient_pipeline.layout, NULL);
  vkDestroyPipeline(ctx->device_ctx.device, ctx->gradient_pipeline.handle, NULL);
  vkDestroyPipelineLayout(ctx->device_ctx.device, ctx->mesh_pipeline.layout, NULL);
  vkDestroyPipeline(ctx->device_ctx.device, ctx->mesh_pipeline.handle, NULL);
  deinit_gltf_meshes(&ctx->device_ctx, ctx->loaded_meshes, ctx->loaded_mesh_count);
  for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
    if (ctx->frames[i].has_scene_data_buffer) {
      destroy_buffer(&ctx->device_ctx.vma, &ctx->frames[i].scene_data_buffer);
      ctx->frames[i].has_scene_data_buffer = false;
    }
  }
  vkDestroySampler(ctx->device_ctx.device, ctx->default_sampler_nearest, NULL);
  vkDestroySampler(ctx->device_ctx.device, ctx->default_sampler_linear, NULL);
  destroy_image(&ctx->device_ctx, &ctx->white_image);
  destroy_image(&ctx->device_ctx, &ctx->grey_image);
  destroy_image(&ctx->device_ctx, &ctx->black_image);
  destroy_image(&ctx->device_ctx, &ctx->error_checkerboard_image);
  vmaDestroyAllocator(ctx->device_ctx.vma);
  /*Gpu Resources deinit ends here*/
  deinit_swapchain(&ctx->device_ctx, &ctx->swapchain_ctx);
  arena_free(&ctx->swapchain_arena);
  deinit_vulkan_context(&ctx->device_ctx);
  SDL_DestroyWindow(ctx->window_ctx.handle);
  SDL_Quit();
}



void draw_background(KutaCtx *ctx, VkCommandBuffer cmd, uint64_t frame_number, uint32_t swapchain_img_idx) {
  float flash = fabsf(sinf(frame_number / 120.0f));
  VkClearColorValue clear_value = { .float32 = { 0.0f, 0.0f, flash, 1.0f } };

  VkImageSubresourceRange clear_range = {
    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
    .baseMipLevel = 0,
    .levelCount = VK_REMAINING_MIP_LEVELS,
    .baseArrayLayer = 0,
    .layerCount = VK_REMAINING_ARRAY_LAYERS,
  };

  vkCmdClearColorImage(cmd, ctx->draw_image.handle, VK_IMAGE_LAYOUT_GENERAL, &clear_value, 1, &clear_range);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, ctx->gradient_pipeline.handle);

  vkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, ctx->gradient_pipeline.layout, 0, 1, &ctx->draw_image_descriptors, 0, NULL);

  vkCmdDispatch(cmd, (uint32_t)ceil(ctx->draw_extent.width / 16.0), (uint32_t)ceil(ctx->draw_extent.height / 16.0), 1);
}

void draw_geometry(Arena *a, KutaCtx *ctx, VkCommandBuffer cmd, FrameCtx *current_frame) {
  VkRenderingAttachmentInfo color_attachment = {
    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
    .pNext = NULL,
    .imageView = ctx->draw_image.view,
    .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
    .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
  };
  VkRenderingAttachmentInfo depth_attachment = {
    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
    .pNext = NULL,
    .imageView = ctx->depth_image.view,
    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
    .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    .clearValue.depthStencil.depth = 0.0f,
  };
  VkRenderingInfo render_info = {
    .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
    .pNext = NULL,
    .renderArea = (VkRect2D){ .offset = (VkOffset2D){ 0, 0 }, .extent = ctx->draw_extent },
    .layerCount = 1,
    .colorAttachmentCount = 1,
    .pColorAttachments = &color_attachment,
    .pDepthAttachment = &depth_attachment,
    .pStencilAttachment = NULL,
  };
  vkCmdBeginRendering(cmd, &render_info);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->mesh_pipeline.handle);

  VkDescriptorSet image_set;
  if (descriptor_allocator_growable_allocate(a, &current_frame->frame_descriptors, ctx->device_ctx.device, ctx->single_image_descriptor_layout, NULL, &image_set)) {
    LOG_E("Failed to allocate image descriptor set");
    return;
  }
  {
    DescriptorWriter writer = {0};
    descriptor_writer_write_image(&writer, 0, ctx->error_checkerboard_image.view, ctx->default_sampler_nearest, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    descriptor_writer_update_set(&writer, ctx->device_ctx.device, image_set);
  }

  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->mesh_pipeline.layout, 0, 1, &image_set, 0, NULL);


  VkViewport viewport = {
    .x = 0,
    .y = 0,
    .width = ctx->draw_extent.width,
    .height = ctx->draw_extent.height,
    .minDepth = 0.1f,
    .maxDepth = 1.0f,
  };
  vkCmdSetViewport(cmd, 0, 1, &viewport);

  VkRect2D scissor = {
    .offset.x = 0,
    .offset.y = 0,
    .extent.width = ctx->draw_extent.width,
    .extent.height = ctx->draw_extent.height,
  };
  vkCmdSetScissor(cmd, 0, 1, &scissor);

  AllocatedBuffer gpu_scene_data_buffer;
  if (create_buffer(&ctx->device_ctx.vma, sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, &gpu_scene_data_buffer)){  
    LOG_E("Failed to create scene data buffer");
    return;
  }

  VmaAllocationInfo alloc_info;
  vmaGetAllocationInfo(ctx->device_ctx.vma, gpu_scene_data_buffer.alloc, &alloc_info);
  GPUSceneData *scene_uniform_data = (GPUSceneData*)alloc_info.pMappedData;
  *scene_uniform_data = ctx->scene_data;

  VkDescriptorSet global_descriptor;
  if (descriptor_allocator_growable_allocate(a, &current_frame->frame_descriptors, ctx->device_ctx.device, ctx->gpu_scene_data_descriptor_layout, NULL, &global_descriptor)) {
    LOG_E("Failed to allocate global scene descriptor");
    destroy_buffer(&ctx->device_ctx.vma, &gpu_scene_data_buffer);
    return;
  }

  DescriptorWriter writer = {0};
  descriptor_writer_write_buffer(&writer, 0, gpu_scene_data_buffer.handle, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  descriptor_writer_update_set(&writer, ctx->device_ctx.device, global_descriptor);

  GPUDrawPushConstants push_constants = {0};
  glm_mat4_identity(push_constants.world_matrix);

  mat4 view;
  glm_translate_make(view, (vec3){ 0.0f, 0.0f, -5.0f });
  mat4 projection;
  glm_perspective(glm_rad(70.0f), (float)ctx->draw_extent.width / (float)ctx->draw_extent.height, 0.1f, 10000.0f, projection);
  projection[1][1] *= -1;
  glm_mat4_mul(projection, view, push_constants.world_matrix);

  push_constants.vertex_buffer = ctx->loaded_meshes[2].mesh_buffers.vertex_buffer_address;
  vkCmdPushConstants(cmd, ctx->mesh_pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &push_constants);
  vkCmdBindIndexBuffer(cmd, ctx->loaded_meshes[2].mesh_buffers.index_buffer.handle, 0, VK_INDEX_TYPE_UINT32);
  vkCmdDrawIndexed(cmd, ctx->loaded_meshes[2].surfaces[0].count, 1, ctx->loaded_meshes[2].surfaces[0].start_index, 0, 0);

  vkCmdEndRendering(cmd);

  current_frame->scene_data_buffer = gpu_scene_data_buffer;
  current_frame->has_scene_data_buffer = true;
}

bool draw(Arena *a, KutaCtx *ctx, uint64_t frame_number) {
  FrameCtx *current_frame = &ctx->frames[frame_number % FRAMES_IN_FLIGHT];

  vkWaitForFences(ctx->device_ctx.device, 1, &current_frame->fence, VK_TRUE, 1000000000);

  descriptor_allocator_growable_clear_pools(a, &current_frame->frame_descriptors, ctx->device_ctx.device);
  if (current_frame->has_scene_data_buffer) {
    destroy_buffer(&ctx->device_ctx.vma, &current_frame->scene_data_buffer);
    current_frame->has_scene_data_buffer = false;
  }
                                                                                                           
  vkResetFences(ctx->device_ctx.device, 1, &current_frame->fence);

  uint32_t swapchain_img_idx;
  VkResult img_idx_res = vkAcquireNextImageKHR(ctx->device_ctx.device, ctx->swapchain_ctx.handle, 1000000000, current_frame->acquire_semaphore, NULL, &swapchain_img_idx);

  if (img_idx_res == VK_ERROR_OUT_OF_DATE_KHR) {
      LOG_I("Window Resize Requrested");
      ctx->window_ctx.resize_requested = true;
      return true;
  }

  VkCommandBuffer cmd = current_frame->cmd;
  if(vkResetCommandBuffer(cmd, 0) != VK_SUCCESS) {
    LOG_E("Failed to reset command buffer");
    return true;
  }

  VkCommandBufferBeginInfo cmd_begin_info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .pNext = NULL,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    .pInheritanceInfo = NULL,
  };


  ctx->draw_extent.width = MIN(ctx->swapchain_ctx.extent.width, ctx->draw_image.extent.width) * ctx->render_scale;
  ctx->draw_extent.height = MIN(ctx->swapchain_ctx.extent.height, ctx->draw_image.extent.height) * ctx->render_scale;

  if(vkBeginCommandBuffer(cmd, &cmd_begin_info) != VK_SUCCESS) {
    LOG_E("Failed ot begin command buffer");
    return true;
  } 
  
  transition_image(cmd, ctx->draw_image.handle, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

  draw_background(ctx, cmd, frame_number, swapchain_img_idx);

  transition_image(cmd, ctx->draw_image.handle, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  transition_image(cmd, ctx->depth_image.handle, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

  draw_geometry(a, ctx, cmd, current_frame);

  transition_image(cmd, ctx->draw_image.handle, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  transition_image(cmd, ctx->swapchain_ctx.images[swapchain_img_idx], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  copy_image_to_image(cmd, ctx->draw_image.handle, ctx->swapchain_ctx.images[swapchain_img_idx], ctx->draw_extent, ctx->swapchain_ctx.extent);

  transition_image(cmd, ctx->swapchain_ctx.images[swapchain_img_idx], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

  if(vkEndCommandBuffer(cmd) != VK_SUCCESS) {
    LOG_E("Failed ot finalize command buffer");
    return true;
  }

  VkCommandBufferSubmitInfo cmd_submit_info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
    .pNext = NULL,
    .commandBuffer = cmd,
    .deviceMask = 0,
  };

  VkSemaphoreSubmitInfo wait_info = {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
    .pNext = NULL,
    .semaphore = current_frame->acquire_semaphore,
    .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
    .deviceIndex = 0,
    .value = 1,
  };

  VkSemaphoreSubmitInfo signal_info = {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
    .pNext = NULL,
    .semaphore = ctx->swapchain_ctx.render_semaphores[swapchain_img_idx],
    .stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
    .deviceIndex = 0,
    .value = 1,
  };

  VkSubmitInfo2 submit = {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
    .pNext = NULL,
    .waitSemaphoreInfoCount = 1,
    .pWaitSemaphoreInfos = &wait_info,
    .signalSemaphoreInfoCount = 1,
    .pSignalSemaphoreInfos = &signal_info,
    .commandBufferInfoCount = 1,
    .pCommandBufferInfos = &cmd_submit_info,
  };

  vkQueueSubmit2(ctx->device_ctx.graphics_queue, 1, &submit, current_frame->fence);

  VkPresentInfoKHR present_info = {
    .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
    .pNext = NULL,
    .swapchainCount = 1,
    .pSwapchains = &ctx->swapchain_ctx.handle,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = &ctx->swapchain_ctx.render_semaphores[swapchain_img_idx],
    .pImageIndices = &swapchain_img_idx,
  };

  VkResult present_res =  vkQueuePresentKHR(ctx->device_ctx.graphics_queue, &present_info);
  if (present_res == VK_ERROR_OUT_OF_DATE_KHR) {
    LOG_I("Window Resize Requrested");
    ctx->window_ctx.resize_requested = true;
  }

  frame_number++;
  return false;
}

void main_loop(Arena *a, KutaCtx *ctx) {
  uint64_t frame_number = 0;
  bool running = true;
  SDL_Event event;
  while (running) {
    while (SDL_PollEvent(&event)) {
          if (event.type == SDL_EVENT_QUIT) {
              running = false;
          }
    }
    if (ctx->window_ctx.resize_requested) {
      resize_swapchain(ctx);
    }
    draw(a, ctx, frame_number);
    frame_number++;
  }
}
