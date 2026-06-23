#pragma once

#include <stdint.h>
#include <stdlib.h>
#include "kuta.h"
#include "kuta_types.h"

typedef struct KtPlatform KtPlatform;

typedef struct {
    KtPlatform *platform;      
    bool        vsync;        
    float       render_scale;
    const char *app_name; 
    uint32_t    api_version;
} KtRendererDesc;

typedef struct {
    KtMat4 view;
    KtMat4 proj;
} KtCameraData;

typedef struct {
    uint32_t mesh_id;
    uint32_t texture_id;
    KtMat4   transform;
} KtDrawCommand;

typedef struct KtRenderer KtRenderer;

struct KtRenderer {
    bool (*begin_frame)(KtRenderer *s);
    void (*end_frame)  (KtRenderer *s);
    void (*set_camera)(KtRenderer *s, KtMat4 view, KtMat4 proj);

    void (*submit)(KtRenderer *self, uint32_t mesh_id, uint32_t mesh_index,
               uint32_t texture_id, KtMat4 transform);

    uint32_t (*load_mesh)    (KtRenderer *s, const char *path);
    uint32_t (*load_texture) (KtRenderer *s, const char *path);
    uint32_t (*upload_mesh)   (KtRenderer *s,
                                const void *vertices, size_t verts_bytes,
                                const void *indices,  size_t idx_bytes);
    uint32_t (*upload_texture)(KtRenderer *s,
                                const uint8_t *pixels,
                                uint32_t w, uint32_t h);

    void (*destroy_mesh)   (KtRenderer *s, uint32_t id);
    void (*destroy_texture)(KtRenderer *s, uint32_t id);

    void (*destroy)(KtRenderer *s);

    void *ctx;
};

KtRenderer kt_vulkan_renderer(const KtRendererDesc *desc);
