#pragma once
#include <cglm/cglm.h>

typedef struct {
    vec3  position;
    vec3  front;
    vec3  up;
    float yaw;
    float pitch;
    float fov;
    float speed;
    float sensitivity;
} Camera;

// GPU-side layout for the scene uniform buffer.
// Uses vec4 throughout to avoid std140 alignment surprises.
typedef struct {
    mat4 view;
    mat4 proj;
    vec4 camera_pos;
} SceneUBO;
