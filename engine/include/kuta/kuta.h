#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef struct KutaCtx KutaCtx;

bool init_engine(KutaCtx *ctx, char* engine_name, char* app_name, char* window_title, uint32_t window_width, uint32_t window_height, uint32_t api_version);

void deinit_engine(KutaCtx *ctx);

void main_loop(KutaCtx *ctx);

KutaCtx* kuta_create();
