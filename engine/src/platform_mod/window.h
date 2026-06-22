#pragma once
#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    SDL_Window *handle;
    uint32_t    width;
    uint32_t    height;
    bool        should_close;
    bool        resize_requested;
} WindowCtx;

bool init_window_context(WindowCtx *ctx, uint32_t width, uint32_t height, char* title);
