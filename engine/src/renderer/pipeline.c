// Pipeline — graphics and compute pipeline builder helpers. #include <vulkan/vulkan.h> #include <stdbool.h>
#include <vulkan/vulkan_core.h>
#include "renderer/pipeline.h"
#include "util/platform.h"
#include "engine.h"
#include "util/log.h"

bool init_pipelines(KutaCtx *ctx) {
  init_background_pipelines(ctx);
  return false;
}
bool init_background_pipelines(KutaCtx *ctx) {
  VkPipelineLayoutCreateInfo compute_layout = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .pNext = NULL,
    .setLayoutCount = 1,
    .pSetLayouts = &ctx->draw_image_descriptor_layout,
  };

  VkResult pip_res = vkCreatePipelineLayout(ctx->device_ctx.device, &compute_layout, NULL, &ctx->gradient_pipeline.layout);
  if(pip_res != VK_SUCCESS) {
    LOG_E("Failed to create compute shader pipeline layout: %d", pip_res);
    return true;
  }

  size_t size;
  VkShaderModule compute_draw_shader;
  if (load_shader_module(&ctx->device_ctx.device, "./shaders/gradient.comp.spv", &size, &compute_draw_shader)) {
    LOG_E("Failed to load shader module");
    return true;
  }

  VkPipelineShaderStageCreateInfo stage_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
    .pNext = NULL,
    .stage = VK_SHADER_STAGE_COMPUTE_BIT,
    .module = compute_draw_shader,
    .pName = "main",
  };

  VkComputePipelineCreateInfo compute_pipeline_create_info = {
    .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
    .pNext = NULL,
    .layout = ctx->gradient_pipeline.layout,
    .stage = stage_info,
  };

  VkResult compute_pip_res = vkCreateComputePipelines(ctx->device_ctx.device, VK_NULL_HANDLE, 1, &compute_pipeline_create_info, NULL, &ctx->gradient_pipeline.handle);
  if(compute_pip_res != VK_SUCCESS) {
    LOG_E("Failed ot create compute pipeline: %d", compute_pip_res);
    return true;
  }

  vkDestroyShaderModule(ctx->device_ctx.device, compute_draw_shader, NULL);

  return false;
}
