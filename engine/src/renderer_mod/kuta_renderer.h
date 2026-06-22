#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "cglm/cglm.h"


typedef struct KtRenderer KtRenderer;

struct KtRenderer {
    bool     (*begin_frame)    (KtRenderer *self);
    void     (*end_frame)      (KtRenderer *self);
    void     (*set_camera)     (KtRenderer *self, mat4 view, mat4 proj);
    void     (*submit)         (KtRenderer *self, uint32_t mesh_id,
                                uint32_t tex_id, mat4 transform);
    uint32_t (*upload_mesh)    (KtRenderer *self, KtMeshData *data);
    uint32_t (*upload_texture) (KtRenderer *self, KtTexData *data);
    void     (*destroy_mesh)   (KtRenderer *self, uint32_t id);
    void     (*destroy)        (KtRenderer *self);

    void *ctx;
};
