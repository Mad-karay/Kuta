#pragma once
#include "renderer/pipeline.h"
#include "renderer/descriptors.h"

typedef struct {
    Pipeline      pipeline;
    DescriptorAllocatorGrowable descriptors;
} Material;
