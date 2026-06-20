#pragma once
#include <vulkan/vulkan.h>
#include <stdbool.h>
#include "util/arena.h"

typedef struct {
    VkPipeline       handle;
    VkPipelineLayout layout;
} Pipeline;

typedef struct KutaCtx KutaCtx;


bool init_pipelines(KutaCtx *ctx);
bool init_background_pipelines(KutaCtx *ctx);
