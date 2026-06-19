#include <stdbool.h>
#include <string.h>
#include "str.h"

static size_t get_str_len(const char *s) {
    size_t length = 0;
    while (s[length] != '\0') length++;
    return length;
}

Str str_from_cstr(const char *s) {
    if (!s) return (Str){0};
    return (Str){ .data = s, .len = get_str_len(s) };
}

Str str_slice(Str s, size_t start, size_t end) {
    return (Str){ .data = s.data + start, .len = end - start };
}

bool str_eq(Str a, Str b) {
    if (a.len != b.len) return false;
    for (size_t i = 0; i < a.len; ++i) {
        if (a.data[i] != b.data[i]) return false;
    }
    return true;
}

bool str_starts_with(Str s, Str prefix) {
    if (prefix.len > s.len) return false;
    for (size_t i = 0; i < prefix.len; ++i) {
        if (s.data[i] != prefix.data[i]) return false;
    }
    return true;
}

Str str_trim(Str s) {
    while (s.len > 0 && s.data[0] == ' ') { s.data++; s.len--; }
    while (s.len > 0 && s.data[s.len - 1] == ' ') { s.len--; }
    return s;
}
