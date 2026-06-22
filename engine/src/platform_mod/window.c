// Window — SDL3 window creation, destruction, and event polling.
#include "platform_mod/window.h"
#include "SDL3/SDL_video.h"
#include "util_mod/log.h"
#include "util_mod/arena.h"
#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>



bool init_window_context(WindowCtx *ctx, uint32_t width, uint32_t height, char* title) {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
        LOG_E("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return true;
    }

    SDL_Window *window = SDL_CreateWindow(title, width, height, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (window == NULL) {
        LOG_E("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_Quit();
        return true;
    }

    ctx->width = width;
    ctx->height = height;
    ctx->handle = window;
    ctx->should_close = 0;

    return false;
}
