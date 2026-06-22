#pragma once
#include "renderer_mod/buffer.h"
#include <stdint.h>
#include "renderer_mod/commands.h"
#include "util_mod/str.h"
#include "renderer_mod/device.h"
#include "cglm/cglm.h"

typedef struct {
    vec3 pos;     
    float uv_x;
    vec3 normal;  
    float uv_y;
    vec4 color;     
} Vertex;

typedef struct {
    AllocatedBuffer vertex_buffer;
    AllocatedBuffer index_buffer;
    uint32_t        index_count;
} Mesh;

typedef struct  {
    AllocatedBuffer index_buffer;
    AllocatedBuffer vertex_buffer;
    VkDeviceAddress vertex_buffer_address;
} GPUMeshBuffers;

typedef struct  {
    mat4 world_matrix;
    VkDeviceAddress vertex_buffer;
} GPUDrawPushConstants;

typedef struct {
    mat4 view;
    mat4 proj;
    mat4 viewproj;
    vec4 ambient_color;
    vec4 sunlight_direction;
    vec4 sunlight_color;
} GPUSceneData;

typedef struct {
    uint32_t start_index;
    uint32_t count;
} GeoSurface;

typedef struct {
    Str             name;
    GeoSurface     *surfaces;
    size_t          surface_count;
    GPUMeshBuffers  mesh_buffers;
} MeshAsset;

bool load_gltf_meshes(Arena *a, DeviceCtx *dev_ctx, ImmediateCtx *imm, const char *filepath,
                       MeshAsset **out_meshes, size_t *out_mesh_count, bool override_colors);

void deinit_gltf_meshes(DeviceCtx *dev_ctx, MeshAsset *meshes, size_t mesh_count);


bool upload_mesh(VmaAllocator *allocator, DeviceCtx *dev_ctx, ImmediateCtx *imm,
                  const uint32_t *indices, size_t index_count,
                  const Vertex *vertices, size_t vertex_count,
                  GPUMeshBuffers *out_surface);
