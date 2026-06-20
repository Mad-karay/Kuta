#include "log.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>

#define INFO_COLOR  "\x1b[38;2;0;255;255m"
#define WARN_COLOR  "\x1b[38;2;255;255;0m"
#define ERROR_COLOR "\x1b[38;2;255;0;0m"
#define RESET       "\x1b[0m"

static const char* level_strings[] = {
    "INFO", "WARN", "ERROR"
};

static const char* level_colors[] = {
    INFO_COLOR,
    WARN_COLOR,
    ERROR_COLOR
};

void log_message(LogLevel level, const char *file, int line, const char *format, ...) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_str[20];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

    fprintf(stderr, "%s %s%-5s%s [%s:%d]: ", 
            time_str, 
            level_colors[level], level_strings[level], RESET,
            file, line);

    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    fprintf(stderr, "\n");
}

