#include <string.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <stdlib.h>
#include <vulkan/vulkan_core.h>
#include "SDL3/SDL_video.h"
#include "kuta/kuta.h"
#include "kuta/kuta_types.h"
#include "kuta/kuta_platform.h"
#include "util_mod/log.h"

typedef struct {
    void    *window;
    float    delta_time;
    float    total_time;
    uint64_t last_tick;
    bool     running;
    bool     key_cur[KT_KEY_COUNT];
    bool     key_prev[KT_KEY_COUNT];
    KtVec2   mouse_pos;
    KtVec2   mouse_delta;
    void *sruface;
} Sdl3Ctx;

static bool sdl3_key_pressed(KtPlatform *s, KtKey k) {
    Sdl3Ctx *c = s->ctx;
    return c->key_cur[k] && !c->key_prev[k];
}

static void sdl3_poll(KtPlatform *pl) {
    Sdl3Ctx *c = pl->ctx;

    memcpy(c->key_prev,
           c->key_cur,
           sizeof(c->key_cur));

    float oldx = c->mouse_pos.x;
    float oldy = c->mouse_pos.y;

    SDL_Event ev;

    while (SDL_PollEvent(&ev))
    {
        switch (ev.type)
        {
        case SDL_EVENT_QUIT:
            c->running = false;
            break;

        case SDL_EVENT_KEY_DOWN:
            if (!ev.key.repeat)
                c->key_cur[ev.key.scancode] = true;
            break;

        case SDL_EVENT_KEY_UP:
            c->key_cur[ev.key.scancode] = false;
            break;
        }
    }


    float mx;
    float my;

    SDL_GetMouseState(&mx, &my);

    c->mouse_pos.x = mx;
    c->mouse_pos.y = my;

    c->mouse_delta.x = mx - oldx;
    c->mouse_delta.y = my - oldy;


    Uint64 now = SDL_GetTicksNS();

    if (c->last_tick)
    {
        c->delta_time =
            (float)(now - c->last_tick)
            / 1000000000.0f;
    }

    c->last_tick = now;
    c->total_time += c->delta_time;
}

static void sdl3_quit(KtPlatform *pl) {
    Sdl3Ctx *c = pl->ctx;
    c->running = false;
}

static bool sdl3_is_running(KtPlatform *pl) {
    Sdl3Ctx *c = pl->ctx;
    return c->running;
}

static float sdl3_delta_time(KtPlatform *pl) {
    return ((Sdl3Ctx*)pl->ctx)->delta_time;
}

static float sdl3_total_time(KtPlatform *pl) {
    return ((Sdl3Ctx*)pl->ctx)->total_time;
}

static void *sdl3_native_window(KtPlatform *pl) {
    return ((Sdl3Ctx*)pl->ctx)->window;
}

static bool sdl3_key_released(KtPlatform *pl,
                              KtKey k) {
    Sdl3Ctx *c = pl->ctx;

    return !c->key_cur[k]
        && c->key_prev[k];
}

static bool sdl3_key_held(KtPlatform *pl,
                          KtKey k) {
    Sdl3Ctx *c = pl->ctx;

    return c->key_cur[k];
}

static KtVec2 sdl3_mouse_pos(KtPlatform *pl) {
    return ((Sdl3Ctx*)pl->ctx)->mouse_pos;
}

static KtVec2 sdl3_mouse_delta(KtPlatform *pl) {
    return ((Sdl3Ctx*)pl->ctx)->mouse_delta;
}

static bool sdl3_mouse_btn(KtPlatform *pl,
                           int btn) {
    SDL_MouseButtonFlags buttons =
        SDL_GetMouseState(NULL,NULL);

    return buttons & SDL_BUTTON_MASK(btn);
}

static void sdl3_destroy(KtPlatform *pl) {
    Sdl3Ctx *c = pl->ctx;

    if (c->window)
        SDL_DestroyWindow(c->window);

    SDL_Quit();

    free(c);

    pl->ctx = NULL;
}



void* sdl3_create_surface(KtPlatform *pl, void *instance) {
  Sdl3Ctx *c = pl->ctx;
  SDL_Window *window = c->window;
  
  VkSurfaceKHR surface = VK_NULL_HANDLE;

  if (!SDL_Vulkan_CreateSurface(window, (VkInstance)instance, NULL, &surface)) {
    LOG_E("Failed to create surface: %s", SDL_GetError());
    return NULL;
  }

  return (void*)surface;
}

void sdl3_get_window_size_pixels(KtPlatform *pl, int *width, int *height) {
  Sdl3Ctx *c = pl->ctx;
  SDL_GetWindowSizeInPixels(c->window, width, height);
}

KtPlatform kt_sdl3_platform(const KtPlatformDesc *d) {
    Sdl3Ctx *c = calloc(1, sizeof(*c));
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        LOG_E("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
    }

    c->window = SDL_CreateWindow(d->window_title, d->window_width, d->window_height, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (c->window == NULL) {
        LOG_E("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_Quit();
    }

    c->running = true;
    return (KtPlatform){
        .poll          = sdl3_poll,
        .quit          = sdl3_quit,
        .is_running    = sdl3_is_running,
        .delta_time    = sdl3_delta_time,
        .key_pressed   = sdl3_key_pressed,
        .key_held      = sdl3_key_held,
        .key_released  = sdl3_key_released,
        .mouse_pos     = sdl3_mouse_pos,
        .mouse_delta   = sdl3_mouse_delta,
        .mouse_btn     = sdl3_mouse_btn,
        .native_window = sdl3_native_window,
        .create_surface = sdl3_create_surface,
        .get_window_size = sdl3_get_window_size_pixels,
        .destroy       = sdl3_destroy,
        .ctx           = c,
    };
}
