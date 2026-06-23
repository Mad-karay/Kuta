#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <stdint.h>
#include <stdbool.h>
#include "util_mod/arena.h"
#include "kuta/kuta_platform.h"

typedef struct {
    VkInstance               instance;
    VkDebugUtilsMessengerEXT debug_messenger;
    VkPhysicalDevice         gpu;
    VkDevice                 device;
    VkSurfaceKHR             surface;
    VkQueue                  graphics_queue;
    uint32_t                 graphics_family;
    VmaAllocator             vma;
} DeviceCtx;

bool init_vulkan_context(Arena *a, DeviceCtx *ctx, KtPlatform *pl, const char* engine_name, const char* app_name, uint32_t api_version);
void deinit_vulkan_context(DeviceCtx *ctx);
