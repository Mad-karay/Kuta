#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include <SDL3/SDL_vulkan.h>
#include "util/log.h"
#include "util/arena.h"
#include "device.h"
#include "vk_mem_alloc.h"


static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
    void *user_data) {
  (void)type;
  (void)user_data;

  if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    LOG_E("[Vulkan] %s", callback_data->pMessage);
  } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    LOG_W("[Vulkan] %s", callback_data->pMessage);
  } else {
    LOG_I("[Vulkan] %s", callback_data->pMessage);
  }

  return VK_FALSE;
}

static VkPhysicalDevice pick_physical_device(VkPhysicalDevice *devices, uint32_t count) {
    VkPhysicalDevice fallback = VK_NULL_HANDLE;

    for (uint32_t i = 0; i < count; ++i) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(devices[i], &props);

        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            return devices[i];
        }
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU && fallback == VK_NULL_HANDLE) {
            fallback = devices[i]; 
        }
    }

    return fallback; 
}

bool init_vulkan_context(Arena *a, DeviceCtx *ctx, SDL_Window *window, char* engine_name, char* app_name, uint32_t api_version) {
  /*INSTANCE CREATION*/
  uint32_t required_extensions_count;
  char const * const * required_extensions = SDL_Vulkan_GetInstanceExtensions(&required_extensions_count);

  uint32_t available_ext_count;
  vkEnumerateInstanceExtensionProperties(NULL, &available_ext_count, NULL);
  VkExtensionProperties *available_extensions = arena_alloc(a, sizeof(VkExtensionProperties) * available_ext_count);
  vkEnumerateInstanceExtensionProperties(NULL, &available_ext_count, available_extensions);

  bool debug_utils_available = false;
  for (uint32_t i = 0; i < available_ext_count; ++i) {
    if (strcmp(available_extensions[i].extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0) {
      debug_utils_available = true;
      break;
    }
  }

  uint32_t total_extension_count = required_extensions_count + (debug_utils_available ? 1 : 0);
  char const **enabled_extensions = arena_alloc(a, sizeof(char*) * total_extension_count);
  for (uint32_t i = 0; i < required_extensions_count; ++i) {
    enabled_extensions[i] = required_extensions[i];
  }
  if (debug_utils_available) {
    enabled_extensions[required_extensions_count] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
  } else {
    LOG_W("VK_EXT_debug_utils not available, skipping debug messenger");
  }

  uint32_t layer_count;
  vkEnumerateInstanceLayerProperties(&layer_count, NULL);

  VkLayerProperties *available_layers = arena_alloc(a, sizeof(VkLayerProperties) * layer_count);
  vkEnumerateInstanceLayerProperties(&layer_count, available_layers);
  
  const char *desired_layers[] = {
    "VK_LAYER_KHRONOS_validation",
  };
  uint32_t desired_layer_count = 1;

  char const **enabled_layers = arena_alloc(a, sizeof(char*) * desired_layer_count);
  uint32_t enabled_layer_count = 0;

  for (uint32_t i = 0; i < desired_layer_count; ++i) {
      bool found = false;
      for (uint32_t j = 0; j < layer_count; ++j) {
          if (strcmp(desired_layers[i], available_layers[j].layerName) == 0) {
              found = true;
              break;
          }
      }
      if (found) {
          enabled_layers[enabled_layer_count++] = desired_layers[i];
      } else {
          LOG_W("Requested layer %s not available", desired_layers[i]);
      }
  }

  VkApplicationInfo app_info = {
    .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
    .pApplicationName = app_name,
    .pEngineName = engine_name,
    .apiVersion = api_version,
  };
  VkInstanceCreateInfo create_info = {
    .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    .pApplicationInfo = &app_info,
    .enabledExtensionCount = total_extension_count,
    .ppEnabledExtensionNames = enabled_extensions,
    .enabledLayerCount = enabled_layer_count,
    .ppEnabledLayerNames = enabled_layers,
  };
  
  VkResult instance_res = vkCreateInstance(&create_info, NULL, &ctx->instance);
  if (instance_res != VK_SUCCESS) {
    LOG_E("Failed to Create Vulkan Instance: %d", instance_res);
    return true;
  }

  /*SELECT PHYSICAL DEVICE*/
  uint32_t pdev_count = 0;
  VkResult pdev_enum_result = vkEnumeratePhysicalDevices(ctx->instance, &pdev_count, NULL) ;
  if (pdev_enum_result != VK_SUCCESS) {
    LOG_E("Failed to enumerate Physical devices: %d", pdev_enum_result);
    return true;
  }
  LOG_I("Found %d supported device", pdev_count);

  if (pdev_count == 0) {
    LOG_E("Failed to find vulkan supported physical devices");
    return true;
  } 
  
  VkPhysicalDevice *devices = arena_alloc(a, sizeof(VkPhysicalDevice) * pdev_count);
  if(!devices) {
    LOG_E("Failed ot allocate Memory for devices");
    return true;
  }

  VkResult pdev_result = vkEnumeratePhysicalDevices(ctx->instance, &pdev_count, devices);
  if (pdev_result != VK_SUCCESS) {
    LOG_E("Failed to enumerate Physical devices"); 
    return true;
  }
 
  for (size_t i = 0; i < pdev_count; ++i) {
      VkPhysicalDeviceProperties props;
      vkGetPhysicalDeviceProperties(devices[i], &props);

      const char *type_str = "UNKNOWN";
      switch (props.deviceType) {
          case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   type_str = "DISCRETE_GPU"; break;
          case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: type_str = "INTEGRATED_GPU"; break;
          case VK_PHYSICAL_DEVICE_TYPE_CPU:            type_str = "CPU"; break;
          case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    type_str = "VIRTUAL_GPU"; break;
          case VK_PHYSICAL_DEVICE_TYPE_OTHER:          type_str = "OTHER"; break;
          default: break;
      }

      LOG_I("Found device: %s (type: %s)", props.deviceName, type_str);
  }

  ctx->gpu = pick_physical_device(devices, pdev_count);
  
  VkPhysicalDeviceProperties props;
  vkGetPhysicalDeviceProperties(ctx->gpu, &props);
  LOG_I("Selected physical Device: %s", props.deviceName);

  /*Create Surface*/
  if(!SDL_Vulkan_CreateSurface(window, ctx->instance, NULL, &ctx->surface)) {
    LOG_E("Failed to create surface %s", SDL_GetError());
    return true;
  }

  /*Select Graphics Queue Family*/
  ctx->graphics_family = UINT32_MAX;
  uint32_t graph_fam_count;

  vkGetPhysicalDeviceQueueFamilyProperties(ctx->gpu, &graph_fam_count, NULL);

  VkQueueFamilyProperties *graph_fam_props = arena_alloc(a, sizeof(VkQueueFamilyProperties) * graph_fam_count);

  if (graph_fam_props == NULL) {
    LOG_E("Error: Failed to allocate memmory for queue families");
    return true;
  }

  vkGetPhysicalDeviceQueueFamilyProperties(ctx->gpu, &graph_fam_count, graph_fam_props);

  for (uint32_t i = 0; i < graph_fam_count; ++i) {
    VkQueueFamilyProperties properties = graph_fam_props[i];
    if (properties.queueFlags & VK_QUEUE_GRAPHICS_BIT && SDL_Vulkan_GetPresentationSupport(ctx->instance, ctx->gpu, i)) {
       ctx->graphics_family = i;
       break;
    }
  }

  if (ctx->graphics_family == UINT32_MAX) {
    LOG_E("Failed no suitable graphics queue family");
    return true;
  }

  /*Create Device*/
  VkPhysicalDeviceFeatures supported_features;
  vkGetPhysicalDeviceFeatures(ctx->gpu,
                              &supported_features);
  VkPhysicalDeviceVulkan13Features features13 = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
    .synchronization2 = VK_TRUE,
    .dynamicRendering = VK_TRUE,
  };

  VkPhysicalDeviceVulkan12Features features12 = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
      .pNext = &features13,
      .bufferDeviceAddress = VK_TRUE,
      .descriptorIndexing = VK_TRUE,
  };

  VkPhysicalDeviceFeatures2 features2 = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
      .pNext = &features12,
      .features = {
          .samplerAnisotropy = VK_TRUE,
          .sampleRateShading = VK_TRUE,
      },
  };

  VkResult dev_result = vkCreateDevice(ctx->gpu, &(VkDeviceCreateInfo) {
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .pQueueCreateInfos =  &(VkDeviceQueueCreateInfo){
                      .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                      .queueFamilyIndex = ctx->graphics_family,
                      .queueCount = 1,
                      .pQueuePriorities = &(float){1.0}},
              .queueCreateInfoCount = 1,
              .enabledExtensionCount = 1,
              .ppEnabledExtensionNames =
                  &(const char *){VK_KHR_SWAPCHAIN_EXTENSION_NAME},
              .pNext = &features2,
      } , NULL, &ctx->device);
  if(dev_result != VK_SUCCESS) {
    LOG_E("Failed to Create a device: %d", dev_result);
    return true;
  }

  /*Get Queue*/
  vkGetDeviceQueue(ctx->device, ctx->graphics_family, 0, &ctx->graphics_queue);

  /*Debug Util*/
  if (debug_utils_available) {
    PFN_vkCreateDebugUtilsMessengerEXT create_debug_messenger =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(ctx->instance, "vkCreateDebugUtilsMessengerEXT");

    if (create_debug_messenger) {
      VkDebugUtilsMessengerCreateInfoEXT debug_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = debug_callback,
      };

      VkResult dbg_res = create_debug_messenger(ctx->instance, &debug_create_info, NULL, &ctx->debug_messenger);
      if (dbg_res != VK_SUCCESS) {
        LOG_W("Failed to create debug messenger: %d", dbg_res);
      }
    } else {
      LOG_W("vkCreateDebugUtilsMessengerEXT not found");
    }
  }
 
  /*VMA Allocator Init*/
  VmaAllocatorCreateInfo allocator_info = {
    .physicalDevice = ctx->gpu,
    .device = ctx->device,
    .instance = ctx->instance,
    .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
  };

  vmaCreateAllocator(&allocator_info, &ctx->vma);

  return false;
}

void deinit_vulkan_context(DeviceCtx *ctx) {
  if (ctx->device != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(ctx->device);
  }

  if (ctx->device != VK_NULL_HANDLE) {
    vkDestroyDevice(ctx->device, NULL);
    ctx->device = VK_NULL_HANDLE;
  }

  if (ctx->debug_messenger != VK_NULL_HANDLE) {
    PFN_vkDestroyDebugUtilsMessengerEXT destroy_debug_messenger =
        (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(ctx->instance, "vkDestroyDebugUtilsMessengerEXT");
    if (destroy_debug_messenger) {
      destroy_debug_messenger(ctx->instance, ctx->debug_messenger, NULL);
    }
    ctx->debug_messenger = VK_NULL_HANDLE;
  }

  if (ctx->surface != VK_NULL_HANDLE) {
    SDL_Vulkan_DestroySurface(ctx->instance, ctx->surface, NULL);
    ctx->surface = VK_NULL_HANDLE;
  }

  if (ctx->instance != VK_NULL_HANDLE) {
    vkDestroyInstance(ctx->instance, NULL);
    ctx->instance = VK_NULL_HANDLE;
  }
}
