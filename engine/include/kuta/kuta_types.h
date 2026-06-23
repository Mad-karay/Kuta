#pragma once
#include <stdint.h>

typedef struct {
  const char *window_title;
  const char *application_name;
  uint32_t window_width, window_height;
  uint32_t api_version;
  float render_scale;
} KutaSettings;

typedef uint32_t  KtEntity;
#define KT_ENTITY_NULL  (KtEntity)0

typedef struct { uint32_t id; } KtMesh;
typedef struct { uint32_t id; } KtTexture;
typedef struct { uint32_t id; } KtSound;
typedef struct { uint32_t id; } KtComponentId;

typedef struct { float x, y; }       KtVec2;
typedef struct { float x, y, z; }    KtVec3;
typedef struct { float x, y, z, w; } KtVec4;
typedef struct { float m[16]; }       KtMat4;

typedef enum {
    KT_OK=0, KT_ERR_PLATFORM, KT_ERR_RENDERER,
    KT_ERR_OOM, KT_ERR_ASSET, KT_ERR_INVALID
} KtResult;

typedef enum KtKey {
  KT_KEY_UNKNOWN = 0,
  KT_KEY_ESCAPE,
} KtKey;

#define KT_KEY_COUNT 28
