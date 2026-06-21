// Entry point — initializes the Engine, runs the main loop, and shuts down.
#include <stdbool.h>
#include <stdint.h>
#include <SDL3/SDL.h>
#include <string.h>
#include <vulkan/vulkan_core.h>
#include "core/window.h"
#include "engine.h"
#include "util/log.h"
#define ARENA_IMPLEMENTATION
#include "util/arena.h"

int main(){
    Arena a = {0};
    KutaCtx *ctx = malloc(sizeof(KutaCtx));

    LOG_I("ctx = %p, sizeof(KutaCtx) = %zu", (void*)ctx, sizeof(KutaCtx));

    memset(ctx, 0, sizeof(KutaCtx));
    
    if(init_engine(&a, ctx, "Kuta", "Kudo", "Hello, Kuta", 800, 600, VK_API_VERSION_1_4)) {
      LOG_E("Init engine failed");
      return 1;
    }

    main_loop(&a, ctx);

    deinit_engine(ctx);
    arena_free(&a);

    free(ctx);
    return 0;
}
