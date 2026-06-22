#pragma once
#include <vulkan/vulkan.h>
#include <stdbool.h>
#include <vulkan/vulkan_core.h>
#include "util_mod/arena.h"

typedef struct {
    VkPipeline       handle;
    VkPipelineLayout layout;
} Pipeline;

bool init_pipelines(VkDevice dev, VkDescriptorSetLayout *draw_image_descriptor_layout, Pipeline *gradient_pipeline);
bool init_background_pipelines(VkDevice dev, VkDescriptorSetLayout *draw_image_descriptor_layout, Pipeline *gradient_pipeline);

typedef struct {
    VkPipelineShaderStageCreateInfo *items;
    size_t                        count;
    size_t                        capacity;
} ShaderStages;

typedef struct {
  ShaderStages shader_stages;
  VkPipelineInputAssemblyStateCreateInfo input_assembly;
  VkPipelineRasterizationStateCreateInfo rasterizer;
  VkPipelineColorBlendAttachmentState color_blend_attachment;
  VkPipelineMultisampleStateCreateInfo multisampling;
  VkPipelineLayout pipeline_layout;
  VkPipelineDepthStencilStateCreateInfo depth_stencil;
  VkPipelineRenderingCreateInfo render_info;
  VkFormat color_attachment_format;
} PipelineBuilder;

VkPipeline build_pipeline(Arena *a, VkDevice *dev, PipelineBuilder *pb);

void pipeline_builder_clear(PipelineBuilder *pb);

bool init_pipeline_builder(PipelineBuilder *pb); 

void pipeline_builder_set_shaders(Arena *a, PipelineBuilder *pb, VkShaderModule vertex_shader, VkShaderModule fragment_shader);

void pipeline_builder_set_input_topology(PipelineBuilder *pb, VkPrimitiveTopology topology);

void pipeline_builder_set_polygon_mode(PipelineBuilder *pb, VkPolygonMode mode);

void pipeline_builder_set_cull_mode(PipelineBuilder *pb, VkCullModeFlags cull_mode, VkFrontFace front_face);

void pipeline_builder_set_multisampling_none(PipelineBuilder *pb);

void pipeline_builder_disable_blending(PipelineBuilder *pb);

void pipeline_builder_set_color_attachment_format(PipelineBuilder *pb, VkFormat format);

void pipeline_builder_set_depth_format(PipelineBuilder *pb, VkFormat format);

void pipeline_builder_disable_depthtest(PipelineBuilder *pb);

void pipeline_builder_enable_depthtest(PipelineBuilder *pb, bool depth_write_enable, VkCompareOp op);

void pipeline_builder_enable_blending_additive(PipelineBuilder *pb);

void pipeline_builder_enable_blending_alphablend(PipelineBuilder *pb);
