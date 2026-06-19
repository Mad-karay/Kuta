#include "log.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>

// Level strings
static const char* level_strings[] = {
    "INFO", "WARN", "ERROR"
};

// ANSI terminal color strings
static const char* level_colors[] = {
    "\x1b[36m", // Cyan
    "\x1b[33m", // Yellow
    "\x1b[31m"  // Red
};
#define COLOR_RESET "\x1b[0m"

void log_message(LogLevel level, const char *file, int line, const char *format, ...) {
    // Get current time
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_str[20];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

    // Print metadata prefix to stderr
    fprintf(stderr, "%s %s%-5s%s [%s:%d]: ", 
            time_str, 
            level_colors[level], level_strings[level], COLOR_RESET,
            file, line);

    // Process variable argument format string
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    // Force a newline
    fprintf(stderr, "\n");
}

