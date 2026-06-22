#pragma once

#include <stdlib.h>
#include <stdbool.h>

typedef struct {
    const char *data;
    size_t len;
} Str;

Str str_from_cstr(const char *s);
Str str_slice(Str s, size_t start, size_t end);
bool str_eq(Str a, Str b);
bool str_starts_with(Str s, Str prefix);
Str str_trim(Str s);
