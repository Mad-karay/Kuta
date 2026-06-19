#pragma once
#include "renderer/pipeline.h"
#include "renderer/descriptors.h"

typedef struct {
    Pipeline      pipeline;
    DescriptorCtx descriptors;
} Material;
