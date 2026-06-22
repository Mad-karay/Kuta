#pragma once
#include <stdint.h>

typedef struct {
  const char *window_title;
  const char *application_name;
  uint32_t window_width, window_height;
  uint32_t api_version;
  float render_scale;
} KutaSettings;
