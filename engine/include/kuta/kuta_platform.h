#pragma once

#include "kuta/kuta.h"
#include "kuta_types.h"
#include <stdbool.h>

typedef struct {
    const char *window_title;
    uint32_t    window_width;
    uint32_t    window_height;
} KtPlatformDesc;

typedef struct KtPlatform {
    void   (*poll)         (struct KtPlatform *s); 
    void   (*quit)         (struct KtPlatform *s);
    bool   (*is_running)   (struct KtPlatform *s);
    void   *(*create_surface)   (struct KtPlatform *s, void *instance);
    void   (*get_window_size)   (struct KtPlatform *s, int *w, int*h);
    float  (*delta_time)   (struct KtPlatform *s);
    float  (*total_time)   (struct KtPlatform *s);
    void  *(*native_window)(struct KtPlatform *s); 

    bool   (*key_pressed) (struct KtPlatform *s, KtKey k); 
    bool   (*key_released)(struct KtPlatform *s, KtKey k);
    bool   (*key_held)    (struct KtPlatform *s, KtKey k);
    KtVec2 (*mouse_pos)   (struct KtPlatform *s);
    KtVec2 (*mouse_delta) (struct KtPlatform *s);
    bool   (*mouse_btn)   (struct KtPlatform *s, int btn);

    void   (*destroy)     (struct KtPlatform *s);
    void  *ctx;
} KtPlatform;

KtPlatform kt_sdl3_platform(const KtPlatformDesc *desc);
