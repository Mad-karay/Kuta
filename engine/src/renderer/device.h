#pragma once
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <stdint.h>

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
