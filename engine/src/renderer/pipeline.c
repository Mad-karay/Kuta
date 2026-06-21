#include <vulkan/vulkan_core.h>
#include "renderer/pipeline.h"
#include "util/platform.h"
#include "util/log.h"


bool init_pipelines(VkDevice dev, VkDescriptorSetLayout *draw_image_descriptor_layout, Pipeline *gradient_pipeline) {
  init_background_pipelines(dev, draw_image_descriptor_layout, gradient_pipeline);
  return false;
}
bool init_background_pipelines(VkDevice dev, VkDescriptorSetLayout *draw_image_descriptor_layout, Pipeline *gradient_pipeline) {
  VkPipelineLayoutCreateInfo compute_layout = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .pNext = NULL,
    .setLayoutCount = 1,
    .pSetLayouts = draw_image_descriptor_layout,
  };

  VkResult pip_res = vkCreatePipelineLayout(dev, &compute_layout, NULL, &gradient_pipeline->layout);
  if(pip_res != VK_SUCCESS) {
    LOG_E("Failed to create compute shader pipeline layout: %d", pip_res);
    return true;
  }

  size_t size;
  VkShaderModule compute_draw_shader;
  if (load_shader_module(&dev, "./shaders/gradient.comp.spv", &size, &compute_draw_shader)) {
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
    .layout = gradient_pipeline->layout,
    .stage = stage_info,
  };

  VkResult compute_pip_res = vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &compute_pipeline_create_info, NULL, &gradient_pipeline->handle);
  if(compute_pip_res != VK_SUCCESS) {
    LOG_E("Failed ot create compute pipeline: %d", compute_pip_res);
    return true;
  }

  vkDestroyShaderModule(dev, compute_draw_shader, NULL);

  return false;
}

bool init_pipeline_builder(PipelineBuilder *pb) {
  pipeline_builder_clear(pb);
  return false;
}

void pipeline_builder_clear(PipelineBuilder *pb) {
  pb->shader_stages.count = 0;
  pb->shader_stages.capacity = 0;
  pb->shader_stages.items = NULL;

  pb->input_assembly = (VkPipelineInputAssemblyStateCreateInfo){
    .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
  };
  
  pb->rasterizer = (VkPipelineRasterizationStateCreateInfo){
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
  };

  pb->color_blend_attachment = (VkPipelineColorBlendAttachmentState){};

  pb->multisampling = (VkPipelineMultisampleStateCreateInfo){
    .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
  };

  pb->pipeline_layout = (VkPipelineLayout){};

  pb->depth_stencil = (VkPipelineDepthStencilStateCreateInfo){
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
  };

  pb->render_info = (VkPipelineRenderingCreateInfo){
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
  };
}


VkPipeline build_pipeline(Arena *a, VkDevice *dev, PipelineBuilder *pb) {
  VkPipelineViewportStateCreateInfo viewport_state = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    .pNext = NULL,
    .viewportCount = 1,
    .scissorCount = 1,

  };

  VkPipelineColorBlendStateCreateInfo color_blending = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    .pNext = NULL,
    .logicOpEnable = VK_FALSE,
    .logicOp = VK_LOGIC_OP_COPY,
    .attachmentCount = 1,
    .pAttachments = &pb->color_blend_attachment,
  };

  VkPipelineVertexInputStateCreateInfo vertex_input_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
  };

  VkDynamicState state[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

  VkPipelineDynamicStateCreateInfo dynamic_info = { 
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO ,
    .dynamicStateCount = 2,
    .pDynamicStates = &state[0],
  };

  VkGraphicsPipelineCreateInfo pipeline_info = { 
    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO ,
    .pNext = &pb->render_info, 
    .stageCount = pb->shader_stages.count,
    .pStages = pb->shader_stages.items,
    .pVertexInputState = &vertex_input_info,
    .pInputAssemblyState = &pb->input_assembly,
    .pViewportState = &viewport_state,
    .pRasterizationState = &pb->rasterizer,
    .pMultisampleState = &pb->multisampling,
    .pColorBlendState = &color_blending,
    .pDepthStencilState = &pb->depth_stencil,
    .layout = pb->pipeline_layout,
    .pDynamicState = &dynamic_info,
  };

  VkPipeline new_pipeline;
  VkResult graph_pip_res = vkCreateGraphicsPipelines(*dev, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &new_pipeline);
  if (graph_pip_res != VK_SUCCESS) {
      LOG_E("Failed to create graphics pipeline: %d", graph_pip_res);
      return VK_NULL_HANDLE; 
  } else {
      return new_pipeline;
  }
}

void pipeline_builder_set_shaders(Arena *a, PipelineBuilder *pb, VkShaderModule vertex_shader, VkShaderModule fragment_shader) {
  pb->shader_stages.count = 0;

  VkPipelineShaderStageCreateInfo vertex_stage = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
    .stage = VK_SHADER_STAGE_VERTEX_BIT,
    .module = vertex_shader,
    .pName = "main",
  };
  arena_da_append(a, &pb->shader_stages, vertex_stage);

  VkPipelineShaderStageCreateInfo fragment_stage = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
    .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
    .module = fragment_shader,
    .pName = "main",
  };
  arena_da_append(a, &pb->shader_stages, fragment_stage);
}

void pipeline_builder_set_input_topology(PipelineBuilder *pb, VkPrimitiveTopology topology) {
  pb->input_assembly.topology = topology;
  pb->input_assembly.primitiveRestartEnable = VK_FALSE;
}

void pipeline_builder_set_polygon_mode(PipelineBuilder *pb, VkPolygonMode mode) {
  pb->rasterizer.polygonMode = mode;
  pb->rasterizer.lineWidth = 1.0f;
}

void pipeline_builder_set_cull_mode(PipelineBuilder *pb, VkCullModeFlags cull_mode, VkFrontFace front_face) {
  pb->rasterizer.cullMode = cull_mode;
  pb->rasterizer.frontFace = front_face;
}

void pipeline_builder_set_multisampling_none(PipelineBuilder *pb) {
  pb->multisampling.sampleShadingEnable = VK_FALSE;
  pb->multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  pb->multisampling.minSampleShading = 1.0f;
  pb->multisampling.pSampleMask = NULL;
  pb->multisampling.alphaToCoverageEnable = VK_FALSE;
  pb->multisampling.alphaToOneEnable = VK_FALSE;
}

void pipeline_builder_disable_blending(PipelineBuilder *pb) {
  pb->color_blend_attachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  pb->color_blend_attachment.blendEnable = VK_FALSE;
}

void pipeline_builder_set_color_attachment_format(PipelineBuilder *pb, VkFormat format) {
  pb->color_attachment_format = format;
  pb->render_info.colorAttachmentCount = 1;
  pb->render_info.pColorAttachmentFormats = &pb->color_attachment_format;
}

void pipeline_builder_set_depth_format(PipelineBuilder *pb, VkFormat format) {
  pb->render_info.depthAttachmentFormat = format;
}

void pipeline_builder_disable_depthtest(PipelineBuilder *pb) {
  pb->depth_stencil.depthTestEnable = VK_FALSE;
  pb->depth_stencil.depthWriteEnable = VK_FALSE;
  pb->depth_stencil.depthCompareOp = VK_COMPARE_OP_NEVER;
  pb->depth_stencil.depthBoundsTestEnable = VK_FALSE;
  pb->depth_stencil.stencilTestEnable = VK_FALSE;
  pb->depth_stencil.front = (VkStencilOpState){0};
  pb->depth_stencil.back = (VkStencilOpState){0};
  pb->depth_stencil.minDepthBounds = 0.0f;
  pb->depth_stencil.maxDepthBounds = 1.0f;
}

void pipeline_builder_enable_depthtest(PipelineBuilder *pb, bool depth_write_enable, VkCompareOp op) {
  pb->depth_stencil.depthTestEnable = VK_TRUE;
  pb->depth_stencil.depthWriteEnable = depth_write_enable;
  pb->depth_stencil.depthCompareOp = op;
  pb->depth_stencil.depthBoundsTestEnable = VK_FALSE;
  pb->depth_stencil.stencilTestEnable = VK_FALSE;
  pb->depth_stencil.front = (VkStencilOpState){0};
  pb->depth_stencil.back = (VkStencilOpState){0};
  pb->depth_stencil.minDepthBounds = 0.0f;
  pb->depth_stencil.maxDepthBounds = 1.0f;
}

void pipeline_builder_enable_blending_additive(PipelineBuilder *pb) {
  pb->color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  pb->color_blend_attachment.blendEnable = VK_TRUE;
  pb->color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  pb->color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
  pb->color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
  pb->color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  pb->color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  pb->color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
}

void pipeline_builder_enable_blending_alphablend(PipelineBuilder *pb) {
  pb->color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  pb->color_blend_attachment.blendEnable = VK_TRUE;
  pb->color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  pb->color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  pb->color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
  pb->color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  pb->color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  pb->color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
}









