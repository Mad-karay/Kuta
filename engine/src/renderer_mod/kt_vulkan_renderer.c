#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <cglm/cglm.h>

#include "renderer_mod/device.h"
#include "renderer_mod/swapchain.h"
#include "renderer_mod/frame.h"
#include "renderer_mod/buffer.h"
#include "renderer_mod/image.h"
#include "renderer_mod/pipeline.h"
#include "renderer_mod/commands.h"
#include "renderer_mod/descriptors.h"
#include "renderer_mod/kt_vulkan_renderer.h"
#include "scene_mod/mesh.h"
#include "util_mod/log.h"
#define ARENA_IMPLEMENTATION
#include "util_mod/arena.h"
#include "util_mod/platform.h"

#include "kuta/kuta_renderer.h"
#include "kuta/kuta_platform.h"


static uint32_t pack_unorm4x8(float r, float g, float b, float a) {
    uint8_t r8 = (uint8_t)(fmaxf(0.0f, fminf(1.0f, r)) * 255.0f + 0.5f);
    uint8_t g8 = (uint8_t)(fmaxf(0.0f, fminf(1.0f, g)) * 255.0f + 0.5f);
    uint8_t b8 = (uint8_t)(fmaxf(0.0f, fminf(1.0f, b)) * 255.0f + 0.5f);
    uint8_t a8 = (uint8_t)(fmaxf(0.0f, fminf(1.0f, a)) * 255.0f + 0.5f);
    return ((uint32_t)a8 << 24) | ((uint32_t)b8 << 16)
         | ((uint32_t)g8 << 8)  | (uint32_t)r8;
}

static uint32_t alloc_mesh_slot(VkRendererCtx *r) {
    for (uint32_t i = 0; i < KT_MAX_MESHES; i++) {
        if (r->meshes[i].asset == NULL) return i + 1;
    }
    return 0;
}

static uint32_t alloc_texture_slot(VkRendererCtx *r) {
    for (uint32_t i = 0; i < KT_MAX_TEXTURES; i++) {
        if (!r->textures[i].alive) return i + 1;
    }
    return 0;
}

static bool init_descriptors(VkRendererCtx *r) {
    Arena *a = &r->perma_arena;

    PoolSizeRatio global_sizes[] = {
        { .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .ratio = 1.0f },
    };
    if (descriptor_allocator_growable_init(a, &r->global_descriptor_allocator,
                                           r->device.device, 10,
                                           global_sizes, 1)) {
        LOG_E("Failed to init global descriptor allocator");
        return true;
    }

    {
        DescriptorLayoutBuilder b = {0};
        descriptor_layout_builder_add_binding(a, &b, 0,
                                              VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        if (descriptor_layout_builder_build(a, &b, r->device.device,
                VK_SHADER_STAGE_COMPUTE_BIT,
                &r->draw_image_descriptor_layout, NULL, 0)) {
            LOG_E("Failed to build draw image descriptor layout");
            return true;
        }
    }

    {
        DescriptorLayoutBuilder b = {0};
        descriptor_layout_builder_add_binding(a, &b, 0,
                                              VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        if (descriptor_layout_builder_build(a, &b, r->device.device,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                &r->scene_data_descriptor_layout, NULL, 0)) {
            LOG_E("Failed to build scene data descriptor layout");
            return true;
        }
    }

    {
        DescriptorLayoutBuilder b = {0};
        descriptor_layout_builder_add_binding(a, &b, 0,
                                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        if (descriptor_layout_builder_build(a, &b, r->device.device,
                VK_SHADER_STAGE_FRAGMENT_BIT,
                &r->single_image_descriptor_layout, NULL, 0)) {
            LOG_E("Failed to build single image descriptor layout");
            return true;
        }
    }

    if (descriptor_allocator_growable_allocate(a,
            &r->global_descriptor_allocator, r->device.device,
            r->draw_image_descriptor_layout, NULL,
            &r->draw_image_descriptors)) {
        LOG_E("Failed to allocate draw image descriptor set");
        return true;
    }

    DescriptorWriter writer = {0};
    descriptor_writer_write_image(&writer, 0,
        r->draw_image.view, VK_NULL_HANDLE,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    descriptor_writer_update_set(&writer, r->device.device,
                                 r->draw_image_descriptors);

    PoolSizeRatio frame_sizes[] = {
        { .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,        .ratio = 3.0f },
        { .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,       .ratio = 3.0f },
        { .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,       .ratio = 3.0f },
        { .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,.ratio = 4.0f },
    };
    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        if (descriptor_allocator_growable_init(a,
                &r->frames[i].frame_descriptors, r->device.device,
                1000, frame_sizes, 4)) {
            LOG_E("Failed to init frame descriptor allocator %u", i);
            return true;
        }
    }

    return false;
}

static bool init_gradient_pipeline(VkRendererCtx *r) {
    Arena *a = &r->perma_arena;

    VkPipelineLayoutCreateInfo layout_info = {
        .sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts    = &r->draw_image_descriptor_layout,
    };
    if (vkCreatePipelineLayout(r->device.device, &layout_info, NULL,
                               &r->gradient_pipeline.layout) != VK_SUCCESS) {
        LOG_E("Failed to create gradient pipeline layout");
        return true;
    }

    size_t sz;
    VkShaderModule comp_shader;
    if (load_shader_module(&r->device.device,
                           "./shaders/gradient.comp.spv",
                           &sz, &comp_shader)) {
        LOG_E("Failed to load gradient compute shader");
        return true;
    }

    VkComputePipelineCreateInfo info = {
        .sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .layout = r->gradient_pipeline.layout,
        .stage  = {
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage  = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = comp_shader,
            .pName  = "main",
        },
    };
    if (vkCreateComputePipelines(r->device.device, VK_NULL_HANDLE, 1,
                                 &info, NULL,
                                 &r->gradient_pipeline.handle) != VK_SUCCESS) {
        LOG_E("Failed to create gradient compute pipeline");
        vkDestroyShaderModule(r->device.device, comp_shader, NULL);
        return true;
    }

    vkDestroyShaderModule(r->device.device, comp_shader, NULL);
    return false;
}

static bool init_mesh_pipeline(VkRendererCtx *r) {
    Arena *a = &r->perma_arena;

    size_t sz;
    VkShaderModule vert, frag;

    if (load_shader_module(&r->device.device,
            "./shaders/colored_triangle_mesh.vert.spv", &sz, &vert)) {
        LOG_E("Failed to load mesh vertex shader"); return true;
    }
    if (load_shader_module(&r->device.device,
            "./shaders/tex_image.frag.spv", &sz, &frag)) {
        LOG_E("Failed to load mesh fragment shader");
        vkDestroyShaderModule(r->device.device, vert, NULL);
        return true;
    }

    VkPushConstantRange pc_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset     = 0,
        .size       = sizeof(GPUDrawPushConstants),
    };

    VkDescriptorSetLayout layouts[] = {
        r->single_image_descriptor_layout,
        r->scene_data_descriptor_layout,
    };
    VkPipelineLayoutCreateInfo layout_info = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount         = 2,
        .pSetLayouts            = layouts,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges    = &pc_range,
    };
    if (vkCreatePipelineLayout(r->device.device, &layout_info, NULL,
                               &r->mesh_pipeline.layout) != VK_SUCCESS) {
        LOG_E("Failed to create mesh pipeline layout");
        vkDestroyShaderModule(r->device.device, vert, NULL);
        vkDestroyShaderModule(r->device.device, frag, NULL);
        return true;
    }

    PipelineBuilder pb = {0};
    init_pipeline_builder(&pb);
    pb.pipeline_layout = r->mesh_pipeline.layout;
    pipeline_builder_set_shaders(a, &pb, vert, frag);
    pipeline_builder_set_input_topology(&pb, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipeline_builder_set_polygon_mode(&pb, VK_POLYGON_MODE_FILL);
    pipeline_builder_set_cull_mode(&pb, VK_CULL_MODE_NONE,
                                   VK_FRONT_FACE_CLOCKWISE);
    pipeline_builder_set_multisampling_none(&pb);
    pipeline_builder_disable_blending(&pb);
    pipeline_builder_enable_depthtest(&pb, true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    pipeline_builder_set_color_attachment_format(&pb, r->draw_image.format);
    pipeline_builder_set_depth_format(&pb, r->depth_image.format);

    r->mesh_pipeline.handle = build_pipeline(a, &r->device.device, &pb);

    vkDestroyShaderModule(r->device.device, vert, NULL);
    vkDestroyShaderModule(r->device.device, frag, NULL);

    if (r->mesh_pipeline.handle == VK_NULL_HANDLE) {
        LOG_E("Failed to build mesh pipeline"); return true;
    }
    return false;
}

static bool init_default_textures(VkRendererCtx *r) {
    uint32_t white  = pack_unorm4x8(1.f, 1.f, 1.f, 1.f);
    uint32_t grey   = pack_unorm4x8(.66f,.66f,.66f,1.f);
    uint32_t black  = pack_unorm4x8(0.f, 0.f, 0.f, 1.f);

    // error checkerboard: magenta + black 16×16
    uint32_t magenta[16*16];
    for (int i = 0; i < 16*16; i++) {
        int x = i % 16, y = i / 16;
        magenta[i] = ((x ^ y) & 1)
            ? pack_unorm4x8(1.f, 0.f, 1.f, 1.f)
            : pack_unorm4x8(0.f, 0.f, 0.f, 1.f);
    }

    VkExtent3D one = {1, 1, 1};
    VkExtent3D big = {16, 16, 1};

    if (create_image_with_data(&r->device, &r->immediate, &white, one,
            VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT,
            false, &r->white_image)) {
        LOG_E("Failed to create white default texture"); return true;
    }
    if (create_image_with_data(&r->device, &r->immediate, &grey, one,
            VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT,
            false, &r->grey_image)) {
        LOG_E("Failed to create grey default texture"); return true;
    }
    if (create_image_with_data(&r->device, &r->immediate, &black, one,
            VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT,
            false, &r->black_image)) {
        LOG_E("Failed to create black default texture"); return true;
    }
    if (create_image_with_data(&r->device, &r->immediate, magenta, big,
            VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT,
            false, &r->error_checkerboard_image)) {
        LOG_E("Failed to create error checkerboard texture"); return true;
    }

    VkSamplerCreateInfo sampler_info = {
        .sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter    = VK_FILTER_NEAREST,
        .minFilter    = VK_FILTER_NEAREST,
    };
    vkCreateSampler(r->device.device, &sampler_info, NULL,
                    &r->sampler_nearest);
    sampler_info.magFilter = VK_FILTER_LINEAR;
    sampler_info.minFilter = VK_FILTER_LINEAR;
    vkCreateSampler(r->device.device, &sampler_info, NULL,
                    &r->sampler_linear);

    return false;
}

static void draw_background(VkRendererCtx *r, VkCommandBuffer cmd) {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                      r->gradient_pipeline.handle);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            r->gradient_pipeline.layout,
                            0, 1, &r->draw_image_descriptors, 0, NULL);
    vkCmdDispatch(cmd,
        (uint32_t)ceilf((float)r->draw_extent.width  / 16.0f),
        (uint32_t)ceilf((float)r->draw_extent.height / 16.0f),
        1);
}

static void flush_draws(VkRendererCtx *r, VkCommandBuffer cmd,
                        FrameCtx *frame) {
    VkRenderingAttachmentInfo color = {
        .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView   = r->draw_image.view,
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp      = VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
    };
    VkRenderingAttachmentInfo depth = {
        .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView   = r->depth_image.view,
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        .loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue.depthStencil.depth = 0.0f,
    };
    VkRenderingInfo ri = {
        .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea           = (VkRect2D){ .offset = {0,0}, .extent = r->draw_extent },
        .layerCount           = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments    = &color,
        .pDepthAttachment     = &depth,
    };
    vkCmdBeginRendering(cmd, &ri);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      r->mesh_pipeline.handle);

    VkViewport viewport = {
        .x = 0, .y = 0,
        .width    = (float)r->draw_extent.width,
        .height   = (float)r->draw_extent.height,
        .minDepth = 0.1f,
        .maxDepth = 1.0f,
    };
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    VkRect2D scissor = {  .offset.x = 0, .offset.y = 0, .extent.width = r->draw_extent.width, .extent.height = r->draw_extent.height };
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    AllocatedBuffer scene_buf;
    if (create_buffer(&r->device.vma, sizeof(GPUSceneData),
                      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                      VMA_MEMORY_USAGE_CPU_TO_GPU, &scene_buf)) {
        LOG_E("Failed to create scene data buffer");
        vkCmdEndRendering(cmd);
        return;
    }

    GPUSceneData *scene_ptr =
        (GPUSceneData *)scene_buf.info.pMappedData;

    glm_mat4_copy(r->cam_view, scene_ptr->view);
    glm_mat4_copy(r->cam_proj, scene_ptr->proj);
    glm_mat4_mul(r->cam_proj, r->cam_view, scene_ptr->view_proj);

    VkDescriptorSet scene_set;
    if (descriptor_allocator_growable_allocate(&r->frame_arena,
            &frame->frame_descriptors, r->device.device,
            r->scene_data_descriptor_layout, NULL, &scene_set)) {
        LOG_E("Failed to allocate scene descriptor set");
        destroy_buffer(&r->device.vma, &scene_buf);
        vkCmdEndRendering(cmd);
        return;
    }
    {
        DescriptorWriter w = {0};
        descriptor_writer_write_buffer(&w, 0, scene_buf.handle,
            sizeof(GPUSceneData), 0,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        descriptor_writer_update_set(&w, r->device.device, scene_set);
    }

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        r->mesh_pipeline.layout,
        1, 1, &scene_set, 0, NULL);

    

    for (uint32_t i = 0; i < r->draw_count; i++) {
        DrawEntry *e = &r->draw_list[i];

        uint32_t mesh_idx = e->mesh_id - 1;
        if (mesh_idx >= KT_MAX_MESHES || r->meshes[mesh_idx].asset == NULL) {
            LOG_E("submit(): invalid mesh_id %u — skipping draw", e->mesh_id);
            continue;
        }
        MeshSlot *slot = &r->meshes[mesh_idx];
        if (e->mesh_index >= slot->count) {
            LOG_E("mesh_index %u out of range (count=%zu) for mesh_id %u",
                  e->mesh_index, slot->count, e->mesh_id);
            continue;
        }
        MeshAsset *mesh = &slot->asset[e->mesh_index];

        AllocatedImage *tex_image = &r->error_checkerboard_image;
        if (e->texture_id != 0) {
            uint32_t tex_idx = e->texture_id - 1;
            if (tex_idx < KT_MAX_TEXTURES && r->textures[tex_idx].alive) {
                tex_image = &r->textures[tex_idx].image;
            }
        }

        VkDescriptorSet image_set;
        if (descriptor_allocator_growable_allocate(&r->frame_arena,
                &frame->frame_descriptors, r->device.device,
                r->single_image_descriptor_layout, NULL, &image_set)) {
            LOG_E("Failed to allocate texture descriptor — skipping");
            continue;
        }
        {
            DescriptorWriter w = {0};
            descriptor_writer_write_image(&w, 0,
                tex_image->view, r->sampler_nearest,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
            descriptor_writer_update_set(&w, r->device.device, image_set);
        }

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
            r->mesh_pipeline.layout,
            0, 1, &image_set, 0, NULL);

        GPUDrawPushConstants pc = {0};
        mat4 view_proj, mvp;
        glm_mat4_mul(r->cam_proj, r->cam_view, view_proj);
        glm_mat4_mul(view_proj, e->transform, mvp);
        glm_mat4_copy(mvp, pc.world_matrix);

        pc.vertex_buffer = mesh->mesh_buffers.vertex_buffer_address;

        vkCmdPushConstants(cmd, r->mesh_pipeline.layout,
                           VK_SHADER_STAGE_VERTEX_BIT,
                           0, sizeof(GPUDrawPushConstants), &pc);

        vkCmdBindIndexBuffer(cmd,
            mesh->mesh_buffers.index_buffer.handle,
            0, VK_INDEX_TYPE_UINT32);

        for (uint32_t s = 0; s < mesh->surface_count; s++) {
            vkCmdDrawIndexed(cmd,
                mesh->surfaces[s].count,
                1,
                mesh->surfaces[s].start_index,
                0, 0);
        }
    }

    vkCmdEndRendering(cmd);

    frame->scene_data_buffer     = scene_buf;
    frame->has_scene_data_buffer = true;
}

static bool vk_begin_frame(KtRenderer *s) {
    VkRendererCtx *r = s->ctx;

    if (r->resize_requested) {
        vkDeviceWaitIdle(r->device.device);
        // destroy old draw/depth images
        vkDestroyImageView(r->device.device, r->draw_image.view, NULL);
        vmaDestroyImage(r->device.vma, r->draw_image.handle, r->draw_image.alloc);
        vkDestroyImageView(r->device.device, r->depth_image.view, NULL);
        vmaDestroyImage(r->device.vma, r->depth_image.handle, r->depth_image.alloc);

        // update window size
        int w, h;
        r->platform->get_window_size(r->platform, &w, &h);

        // reset swapchain arena and recreate
        arena_reset(&r->swapchain_arena);
        deinit_swapchain(&r->device, &r->swapchain);
        if (init_swapchain_ctx(&r->swapchain_arena, &r->swapchain, &r->device,
                               r->platform, &r->draw_image, &r->depth_image)) {
            LOG_E("Failed to recreate swapchain on resize");
            return false;
        }
        r->resize_requested = false;
    }

    FrameCtx *frame = &r->frames[r->frame_number % FRAMES_IN_FLIGHT];

    vkWaitForFences(r->device.device, 1, &frame->fence, VK_TRUE, 1000000000);

    descriptor_allocator_growable_clear_pools(&r->frame_arena,
        &frame->frame_descriptors, r->device.device);
    if (frame->has_scene_data_buffer) {
        destroy_buffer(&r->device.vma, &frame->scene_data_buffer);
        frame->has_scene_data_buffer = false;
    }
    arena_free(&r->frame_arena);

    vkResetFences(r->device.device, 1, &frame->fence);

    uint32_t img_idx;
    VkResult res = vkAcquireNextImageKHR(r->device.device,
        r->swapchain.handle, 1000000000,
        frame->acquire_semaphore, VK_NULL_HANDLE, &img_idx);

    if (res == VK_ERROR_OUT_OF_DATE_KHR) {
        r->resize_requested = true;
        return false;
    }

    r->frame_index = img_idx;

    r->draw_extent.width  = (uint32_t)((float)
        (r->swapchain.extent.width  < r->draw_image.extent.width
         ? r->swapchain.extent.width  : r->draw_image.extent.width)
         * r->render_scale);
    r->draw_extent.height = (uint32_t)((float)
        (r->swapchain.extent.height < r->draw_image.extent.height
         ? r->swapchain.extent.height : r->draw_image.extent.height)
         * r->render_scale);

    VkCommandBuffer cmd = frame->cmd;
    vkResetCommandBuffer(cmd, 0);
    VkCommandBufferBeginInfo begin = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    vkBeginCommandBuffer(cmd, &begin);

    transition_image(cmd, r->draw_image.handle,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    draw_background(r, cmd);

    transition_image(cmd, r->draw_image.handle,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    transition_image(cmd, r->depth_image.handle,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    return true;
}

static void vk_end_frame(KtRenderer *s) {
    VkRendererCtx *r  = s->ctx;
    uint32_t       fi = r->frame_number % FRAMES_IN_FLIGHT;
    FrameCtx      *frame = &r->frames[fi];
    VkCommandBuffer cmd  = frame->cmd;

    flush_draws(r, cmd, frame);

    r->draw_count = 0;
    r->has_camera = false;
    glm_mat4_identity(r->cam_view);
    glm_mat4_identity(r->cam_proj);

    uint32_t img_idx = r->frame_index;
    transition_image(cmd, r->draw_image.handle,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    transition_image(cmd, r->swapchain.images[img_idx],
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    copy_image_to_image(cmd,
        r->draw_image.handle,
        r->swapchain.images[img_idx],
        r->draw_extent, r->swapchain.extent);

    transition_image(cmd, r->swapchain.images[img_idx],
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    vkEndCommandBuffer(cmd);

    VkCommandBufferSubmitInfo cmd_info = {
        .sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = cmd,
    };
    VkSemaphoreSubmitInfo wait_info = {
        .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = frame->acquire_semaphore,
        .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
        .value     = 1,
    };
    VkSemaphoreSubmitInfo signal_info = {
        .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = r->swapchain.render_semaphores[img_idx],
        .stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
        .value     = 1,
    };
    VkSubmitInfo2 submit = {
        .sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .waitSemaphoreInfoCount   = 1,
        .pWaitSemaphoreInfos      = &wait_info,
        .signalSemaphoreInfoCount = 1,
        .pSignalSemaphoreInfos    = &signal_info,
        .commandBufferInfoCount   = 1,
        .pCommandBufferInfos      = &cmd_info,
    };
    vkQueueSubmit2(r->device.graphics_queue, 1, &submit, frame->fence);

    VkPresentInfoKHR present = {
        .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .swapchainCount     = 1,
        .pSwapchains        = &r->swapchain.handle,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores    = &r->swapchain.render_semaphores[img_idx],
        .pImageIndices      = &img_idx,
    };
    VkResult res = vkQueuePresentKHR(r->device.graphics_queue, &present);
    if (res == VK_ERROR_OUT_OF_DATE_KHR) {
        r->resize_requested = true;
    }

    r->frame_number++;
}

static void vk_set_camera(KtRenderer *s, KtMat4 view, KtMat4 proj) {
    VkRendererCtx *r = s->ctx;
    memcpy(r->cam_view, view.m, sizeof(mat4));
    memcpy(r->cam_proj, proj.m, sizeof(mat4));
    r->has_camera = true;
}

static void vk_submit(KtRenderer *s, uint32_t mesh_id, uint32_t mesh_index,
                      uint32_t tex_id, KtMat4 transform) {
    VkRendererCtx *r = s->ctx;
    if (r->draw_count >= KT_MAX_DRAWS) {
        LOG_E("KT_MAX_DRAWS exceeded"); return;
    }
    DrawEntry *e = &r->draw_list[r->draw_count++];
    e->mesh_id    = mesh_id;
    e->mesh_index = mesh_index;
    e->texture_id = tex_id;
    memcpy(e->transform, transform.m, sizeof(mat4));
}


static uint32_t vk_load_mesh(KtRenderer *s, const char *path) {
    VkRendererCtx *r = s->ctx;

    uint32_t slot_id = alloc_mesh_slot(r);
    if (slot_id == 0) {
        LOG_E("vk_load_mesh: mesh table full (%d slots)", KT_MAX_MESHES);
        return 0;
    }

    MeshAsset *asset = calloc(1, sizeof(MeshAsset));
    size_t count = 0;

    if (load_gltf_meshes(&r->perma_arena, &r->device, &r->immediate,
                         path, &asset, &count, true)) {
        LOG_E("vk_load_mesh: failed to load '%s'", path);
        free(asset);
        return 0;
    }

    r->meshes[slot_id - 1].asset = asset;
    r->meshes[slot_id - 1].count = count;
    return slot_id;
}

static uint32_t vk_load_texture(KtRenderer *s, const char *path) {
    // TODO: load PNG/JPG with stb_image, call create_image_with_data
    // Returning 0 (invalid) until stb_image integration is added.
    (void)s; (void)path;
    LOG_E("vk_load_texture: not yet implemented — use error texture");
    return 0;
}

static uint32_t vk_upload_mesh(KtRenderer *s,
                               const void *vertices, size_t verts_bytes,
                               const void *indices,  size_t idx_bytes) {
    // TODO: build a MeshAsset from raw vertex/index data
    (void)s; (void)vertices; (void)verts_bytes;
    (void)indices; (void)idx_bytes;
    LOG_E("vk_upload_mesh: not yet implemented");
    return 0;
}

static uint32_t vk_upload_texture(KtRenderer *s, const uint8_t *pixels,
                                   uint32_t w, uint32_t h) {
    VkRendererCtx *r = s->ctx;

    uint32_t slot_id = alloc_texture_slot(r);
    if (slot_id == 0) {
        LOG_E("vk_upload_texture: texture table full");
        return 0;
    }

    AllocatedImage img;
    VkExtent3D ext = { w, h, 1 };
    if (create_image_with_data(&r->device, &r->immediate,
                               (void*)pixels, ext,
                               VK_FORMAT_R8G8B8A8_UNORM,
                               VK_IMAGE_USAGE_SAMPLED_BIT,
                               false, &img)) {
        LOG_E("vk_upload_texture: create_image_with_data failed");
        return 0;
    }

    r->textures[slot_id - 1].image = img;
    r->textures[slot_id - 1].alive = true;
    return slot_id;
}

static void vk_destroy_mesh(KtRenderer *s, uint32_t id) {
    VkRendererCtx *r = s->ctx;
    if (id == 0 || id > KT_MAX_MESHES) return;
    MeshSlot *slot = &r->meshes[id - 1];
    if (slot->asset) {
        vkDeviceWaitIdle(r->device.device);
        deinit_gltf_meshes(&r->device, slot->asset, slot->count);
        slot->asset = NULL;
    }
}

static void vk_destroy_texture(KtRenderer *s, uint32_t id) {
    VkRendererCtx *r = s->ctx;
    if (id == 0 || id > KT_MAX_TEXTURES) return;
    TextureSlot *slot = &r->textures[id - 1];
    if (slot->alive) {
        vkDeviceWaitIdle(r->device.device);
        destroy_image(&r->device, &slot->image);
        slot->alive = false;
    }
}

static void vk_destroy(KtRenderer *s) {
    VkRendererCtx *r = s->ctx;

    vkDeviceWaitIdle(r->device.device);

    for (uint32_t i = 0; i < KT_MAX_MESHES; i++) {
        if (r->meshes[i].asset) {
            deinit_gltf_meshes(&r->device, r->meshes[i].asset, r->meshes[i].count);
            r->meshes[i].asset = NULL;
        }
    }

    for (uint32_t i = 0; i < KT_MAX_TEXTURES; i++) {
        if (r->textures[i].alive) {
            destroy_image(&r->device, &r->textures[i].image);
            r->textures[i].alive = false;
        }
    }

    destroy_image(&r->device, &r->white_image);
    destroy_image(&r->device, &r->grey_image);
    destroy_image(&r->device, &r->black_image);
    destroy_image(&r->device, &r->error_checkerboard_image);
    vkDestroySampler(r->device.device, r->sampler_linear,  NULL);
    vkDestroySampler(r->device.device, r->sampler_nearest, NULL);

    vkDestroyPipeline      (r->device.device, r->gradient_pipeline.handle, NULL);
    vkDestroyPipelineLayout(r->device.device, r->gradient_pipeline.layout, NULL);
    vkDestroyPipeline      (r->device.device, r->mesh_pipeline.handle,     NULL);
    vkDestroyPipelineLayout(r->device.device, r->mesh_pipeline.layout,     NULL);

    vkDestroyDescriptorSetLayout(r->device.device,
        r->draw_image_descriptor_layout, NULL);
    vkDestroyDescriptorSetLayout(r->device.device,
        r->scene_data_descriptor_layout, NULL);
    vkDestroyDescriptorSetLayout(r->device.device,
        r->single_image_descriptor_layout, NULL);

    descriptor_allocator_growable_destroy_pools(
        &r->global_descriptor_allocator, r->device.device);
    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        descriptor_allocator_growable_destroy_pools(
            &r->frames[i].frame_descriptors, r->device.device);
    }

    deinit_frame_commands(&r->device, r->frames, FRAMES_IN_FLIGHT);

    destroy_image(&r->device, &r->draw_image);
    destroy_image(&r->device, &r->depth_image);

    deinit_swapchain(&r->device, &r->swapchain);

    vkDestroyFence(r->device.device, r->immediate.fence, NULL);
    vkDestroyCommandPool(r->device.device, r->immediate.pool, NULL);

    arena_free(&r->perma_arena);
    arena_free(&r->swapchain_arena);

    free(r);
    s->ctx = NULL;
}

KtRenderer kt_vulkan_renderer(const KtRendererDesc *desc) {
    VkRendererCtx *r = calloc(1, sizeof(VkRendererCtx));

    KtRendererDesc d = desc ? *desc : (KtRendererDesc){0};
    if (!d.app_name)    d.app_name    = "Kuta App";
    if (!d.api_version) d.api_version = VK_API_VERSION_1_3;
    if (d.render_scale == 0.0f) d.render_scale = 1.0f;

    r->platform     = d.platform;
    r->render_scale = d.render_scale;


    if (init_vulkan_context(&r->perma_arena, &r->device,
                            d.platform,
                            "Kuta",  d.app_name, d.api_version)) {
        LOG_E("kt_vulkan_renderer: failed to init Vulkan context");
        free(r);
        return (KtRenderer){0};
    }

    if (init_swapchain_ctx(&r->swapchain_arena, &r->swapchain,
                           &r->device, d.platform,
                           &r->draw_image, &r->depth_image)) {
        LOG_E("kt_vulkan_renderer: failed to init swapchain");
        free(r);
        return (KtRenderer){0};
    }

    if (init_frame_commands(&r->device, r->frames, FRAMES_IN_FLIGHT)) {
        LOG_E("kt_vulkan_renderer: failed to init frame commands");
        free(r);
        return (KtRenderer){0};
    }

    if (init_immediate_commands(&r->device, &r->immediate)) {
        LOG_E("kt_vulkan_renderer: failed to init immediate commands");
        free(r);
        return (KtRenderer){0};
    }

    if (init_descriptors(r)) {
        LOG_E("kt_vulkan_renderer: failed to init descriptors");
        free(r);
        return (KtRenderer){0};
    }

    if (init_gradient_pipeline(r)) {
        LOG_E("kt_vulkan_renderer: failed to init gradient pipeline");
        free(r);
        return (KtRenderer){0};
    }

    if (init_mesh_pipeline(r)) {
        LOG_E("kt_vulkan_renderer: failed to init mesh pipeline");
        free(r);
        return (KtRenderer){0};
    }

    if (init_default_textures(r)) {
        LOG_E("kt_vulkan_renderer: failed to create default textures");
        free(r);
        return (KtRenderer){0};
    }

    return (KtRenderer){
        .begin_frame    = vk_begin_frame,
        .end_frame      = vk_end_frame,
        .set_camera     = vk_set_camera,
        .submit         = vk_submit,
        .load_mesh      = vk_load_mesh,
        .load_texture   = vk_load_texture,
        .upload_mesh    = vk_upload_mesh,
        .upload_texture = vk_upload_texture,
        .destroy_mesh   = vk_destroy_mesh,
        .destroy_texture= vk_destroy_texture,
        .destroy        = vk_destroy,
        .ctx            = r,
    };
}
