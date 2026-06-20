#include "util/log.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <vulkan/vulkan.h>
#include <stdint.h>
#include <vulkan/vulkan_core.h>

bool load_shader_module(VkDevice *dev, const char *filename, size_t *size, VkShaderModule *out_module) {
  FILE *file = fopen(filename, "rb");
  if (!file) {
    fprintf(stderr, "Failed to open file: %s\n", filename);
    return true;
  }

  fseek(file, 0, SEEK_END);
  long file_size = ftell(file);
  rewind(file);

  if (file_size <= 0 || file_size % 4 != 0) {
    fprintf(stderr, "Invalid SPIR-V file size: %ld\n", file_size);
    fclose(file);
    return true;
  }

  uint32_t *buffer = malloc(file_size);
  if (!buffer) {
    fprintf(stderr, "Failed to allocate memory for shader\n");
    fclose(file);
    return true;
  }

  size_t read_size = fread(buffer, 1, file_size, file);
  fclose(file);

  if (read_size != (size_t)file_size) {
    fprintf(stderr, "Failed to read entire file: %s\n", filename);
    free(buffer);
    return true;
  }

  *size = (size_t)file_size;

  VkShaderModuleCreateInfo create_info = {
    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    .pNext = NULL,
    .codeSize = read_size,
    .pCode = buffer,
  };

  VkShaderModule shader_module;

  VkResult shader_mod_res = vkCreateShaderModule(*dev, &create_info, NULL, &shader_module);
  if (shader_mod_res != VK_SUCCESS) {
        LOG_E("Failed to create shader Module: %d", shader_mod_res);
        return true;
  }
  
  *out_module = shader_module;
  return false;
}
