// Entry point — initializes the Engine, runs the main loop, and shuts down.
#include <stdbool.h>
#include <stdint.h>
#include <SDL3/SDL.h>
#include <vulkan/vulkan_core.h>
#include "core/window.h"
#include "engine.h"
#define ARENA_IMPLEMENTATION
#include "util/arena.h"

int main(){
    Arena a = {0};
    KutaCtx *ctx = arena_alloc(&a, sizeof(KutaCtx));
    
    init_engine(&a, ctx, "Kuta", "Kudo", "Hello, Kuta", 800, 600, VK_API_VERSION_1_4);

    main_loop(ctx);

    deinit_engine(ctx);
    arena_free(&a);
    return 0;
}
