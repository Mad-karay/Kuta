#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <vulkan/vulkan.h>

bool load_shader_module(VkDevice *dev, const char *filename, size_t *size, VkShaderModule *out_module);
