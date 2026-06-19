#pragma once
#include "renderer/buffer.h"
#include <stdint.h>

typedef struct {
    float x, y, z;     // position
    float nx, ny, nz;  // normal
    float u, v;        // uv
} Vertex;

typedef struct {
    AllocatedBuffer vertex_buffer;
    AllocatedBuffer index_buffer;
    uint32_t        index_count;
} Mesh;
