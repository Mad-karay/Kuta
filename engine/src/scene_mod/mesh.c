
#include <stdbool.h>
#include <string.h>
#include "scene_mod/mesh.h"
#include "renderer_mod/buffer.h"
#include "renderer_mod/commands.h"
#include "renderer_mod/device.h"
#include "util_mod/arena.h"
#include "util_mod/log.h"
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"


typedef struct {
    uint32_t *items;
    size_t    count;
    size_t    capacity;
} U32Array;

typedef struct {
    Vertex *items;
    size_t  count;
    size_t  capacity;
} VertexArray;

bool load_gltf_meshes(Arena *a, DeviceCtx *dev_ctx, ImmediateCtx *imm, const char *filepath,
                       MeshAsset **out_meshes, size_t *out_mesh_count, bool override_colors) {
  cgltf_options options = {0};
  cgltf_data *data = NULL;

  if (cgltf_parse_file(&options, filepath, &data) != cgltf_result_success) {
    LOG_E("Failed to parse glTF file: %s", filepath);
    return true;
  }

  if (cgltf_load_buffers(&options, data, filepath) != cgltf_result_success) {
    LOG_E("Failed to load glTF buffers: %s", filepath);
    cgltf_free(data);
    return true;
  }

  MeshAsset *meshes = arena_alloc(a, sizeof(MeshAsset) * data->meshes_count);
  if (!meshes) {
    LOG_E("Failed to allocate memory for meshes");
    cgltf_free(data);
    return true;
  }

  for (cgltf_size mesh_i = 0; mesh_i < data->meshes_count; mesh_i++) {
    cgltf_mesh *src_mesh = &data->meshes[mesh_i];
    MeshAsset *dst_mesh = &meshes[mesh_i];

    dst_mesh->name = str_from_cstr(src_mesh->name ? src_mesh->name : "unnamed");
    dst_mesh->surfaces = arena_alloc(a, sizeof(GeoSurface) * src_mesh->primitives_count);
    dst_mesh->surface_count = src_mesh->primitives_count;

    U32Array indices = {0};
    VertexArray vertices = {0};

    for (cgltf_size prim_i = 0; prim_i < src_mesh->primitives_count; prim_i++) {
      cgltf_primitive *prim = &src_mesh->primitives[prim_i];

      GeoSurface surface = {
        .start_index = (uint32_t)indices.count,
        .count = (uint32_t)prim->indices->count,
      };
      dst_mesh->surfaces[prim_i] = surface;

      size_t initial_vertex = vertices.count;

      for (cgltf_size i = 0; i < prim->indices->count; i++) {
        uint32_t idx = (uint32_t)(initial_vertex + cgltf_accessor_read_index(prim->indices, i));
        arena_da_append(a, &indices, idx);
      }

      /*Load positions */
      const cgltf_accessor *pos_accessor = cgltf_find_accessor(prim, cgltf_attribute_type_position, 0);
      if (!pos_accessor) {
        LOG_E("Primitive %zu of mesh %s has no POSITION attribute", (size_t)prim_i, src_mesh->name ? src_mesh->name : "unnamed");
        cgltf_free(data);
        return true;
      }

      for (cgltf_size v = 0; v < pos_accessor->count; v++) {
        Vertex new_vtx = {0};
        float pos[3] = {0};
        cgltf_accessor_read_float(pos_accessor, v, pos, 3);
        new_vtx.pos[0] = pos[0];
        new_vtx.pos[1] = pos[1];
        new_vtx.pos[2] = pos[2];
        new_vtx.normal[0] = 1.0f; new_vtx.normal[1] = 0.0f; new_vtx.normal[2] = 0.0f;
        new_vtx.color[0] = 1.0f; new_vtx.color[1] = 1.0f; new_vtx.color[2] = 1.0f; new_vtx.color[3] = 1.0f;
        new_vtx.uv_x = 0.0f;
        new_vtx.uv_y = 0.0f;
        arena_da_append(a, &vertices, new_vtx);
      }

      /*Load normals, overwriting the placeholder if present*/
      const cgltf_accessor *normal_accessor = cgltf_find_accessor(prim, cgltf_attribute_type_normal, 0);
      if (normal_accessor) {
        for (cgltf_size v = 0; v < normal_accessor->count; v++) {
          float n[3];
          cgltf_accessor_read_float(normal_accessor, v, n, 3);
          vertices.items[initial_vertex + v].normal[0] = n[0];
          vertices.items[initial_vertex + v].normal[1] = n[1];
          vertices.items[initial_vertex + v].normal[2] = n[2];

          if (override_colors) {
            vertices.items[initial_vertex + v].color[0] = n[0];
            vertices.items[initial_vertex + v].color[1] = n[1];
            vertices.items[initial_vertex + v].color[2] = n[2];
            vertices.items[initial_vertex + v].color[3] = 1.0f;
          }
        }
      }

      /*Load UVs*/
      const cgltf_accessor *uv_accessor = cgltf_find_accessor(prim, cgltf_attribute_type_texcoord, 0);
      if (uv_accessor) {
        for (cgltf_size v = 0; v < uv_accessor->count; v++) {
          float uv[2];
          cgltf_accessor_read_float(uv_accessor, v, uv, 2);
          vertices.items[initial_vertex + v].uv_x = uv[0];
          vertices.items[initial_vertex + v].uv_y = uv[1];
        }
      }

      /*Load vertex colors*/
      const cgltf_accessor *color_accessor = cgltf_find_accessor(prim, cgltf_attribute_type_color, 0);
      if (color_accessor) {
        for (cgltf_size v = 0; v < color_accessor->count; v++) {
          float c[4];
          cgltf_accessor_read_float(color_accessor, v, c, 4);
          vertices.items[initial_vertex + v].color[0] = c[0];
          vertices.items[initial_vertex + v].color[1] = c[1];
          vertices.items[initial_vertex + v].color[2] = c[2];
          vertices.items[initial_vertex + v].color[3] = c[3];
        }
      }
    }

    if (upload_mesh(&dev_ctx->vma, dev_ctx, imm,
                     indices.items, indices.count,
                     vertices.items, vertices.count,
                     &dst_mesh->mesh_buffers)) {
      LOG_E("Failed to upload mesh: %.*s", (int)dst_mesh->name.len, dst_mesh->name.data);
      cgltf_free(data);
      return true;
    }
  }

  *out_meshes = meshes;
  *out_mesh_count = data->meshes_count;
  cgltf_free(data);
  return false;
}

void deinit_gltf_meshes(DeviceCtx *dev_ctx, MeshAsset *meshes, size_t mesh_count) {
  for (size_t i = 0; i < mesh_count; i++) {
    destroy_buffer(&dev_ctx->vma, &meshes[i].mesh_buffers.index_buffer);
    destroy_buffer(&dev_ctx->vma, &meshes[i].mesh_buffers.vertex_buffer);
  }
}

typedef struct {
    AllocatedBuffer *staging;
    AllocatedBuffer *vertex_buffer;
    AllocatedBuffer *index_buffer;
    size_t           vertex_buffer_size;
    size_t           index_buffer_size;
} MeshCopyArgs;


static void copy_mesh_buffers(VkCommandBuffer cmd, void *user_data) {
  MeshCopyArgs *args = (MeshCopyArgs*)user_data;

  VkBufferCopy vertex_copy = {
    .dstOffset = 0,
    .srcOffset = 0,
    .size = args->vertex_buffer_size,
  };
  vkCmdCopyBuffer(cmd, args->staging->handle, args->vertex_buffer->handle, 1, &vertex_copy);

  VkBufferCopy index_copy = {
    .dstOffset = 0,
    .srcOffset = args->vertex_buffer_size,
    .size = args->index_buffer_size,
  };
  vkCmdCopyBuffer(cmd, args->staging->handle, args->index_buffer->handle, 1, &index_copy);
}

bool upload_mesh(VmaAllocator *allocator, DeviceCtx *dev_ctx, ImmediateCtx *imm,
                  const uint32_t *indices, size_t index_count,
                  const Vertex *vertices, size_t vertex_count,
                  GPUMeshBuffers *out_surface) {
  size_t vertex_buffer_size = vertex_count * sizeof(Vertex);
  size_t index_buffer_size = index_count * sizeof(uint32_t);

  if (create_buffer(allocator, vertex_buffer_size,
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
      VMA_MEMORY_USAGE_GPU_ONLY, &out_surface->vertex_buffer)) {
    LOG_E("Failed to create vertex buffer");
    return true;
  }

  VkBufferDeviceAddressInfo device_address_info = {
    .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
    .buffer = out_surface->vertex_buffer.handle,
  };
  out_surface->vertex_buffer_address = vkGetBufferDeviceAddress(dev_ctx->device, &device_address_info);

  if (create_buffer(allocator, index_buffer_size,
      VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      VMA_MEMORY_USAGE_GPU_ONLY, &out_surface->index_buffer)) {
    LOG_E("Failed to create index buffer");
    return true;
  }

  AllocatedBuffer staging;
  if (create_buffer(allocator, vertex_buffer_size + index_buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, &staging)) {
    LOG_E("Failed to create staging buffer");
    return true;
  }

  VmaAllocationInfo alloc_info;
  vmaGetAllocationInfo(*allocator, staging.alloc, &alloc_info);
  void *data = alloc_info.pMappedData;

  memcpy((char*)data, vertices, vertex_buffer_size);

  memcpy((char*)data + vertex_buffer_size, indices, index_buffer_size);

  MeshCopyArgs copy_args = {
    .staging = &staging,
    .vertex_buffer = &out_surface->vertex_buffer,
    .index_buffer = &out_surface->index_buffer,
    .vertex_buffer_size = vertex_buffer_size,
    .index_buffer_size = index_buffer_size,
  };

  if (immediate_submit(dev_ctx, imm, copy_mesh_buffers, &copy_args)) {
    LOG_E("Failed to submit mesh buffer copy");
    destroy_buffer(allocator, &staging);
    return true;
  }

  destroy_buffer(allocator, &staging);

  return false;

  return false;
}

