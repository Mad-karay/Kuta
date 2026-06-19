#pragma once
#include <SDL3/SDL.h>
#include <stdbool.h>

typedef struct {
    bool  keys[SDL_SCANCODE_COUNT];
    float mouse_dx;
    float mouse_dy;
    bool  first_mouse;
} InputState;
