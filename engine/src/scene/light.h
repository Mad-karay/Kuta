#pragma once
#include <cglm/cglm.h>

// vec4 layout: xyz = position, w = type (0 = directional, 1 = point)
// vec4 layout: xyz = color,    w = intensity
typedef struct {
    vec4 position;
    vec4 color;
} Light;
