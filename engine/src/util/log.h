#pragma once

// Log levels
typedef enum {
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR
} LogLevel;

// Core logging function called by macros
void log_message(LogLevel level, const char *file, int line, const char *format, ...);

// User-facing macros to automatically capture file and line numbers
#define LOG_I(fmt, ...) log_message(LOG_INFO,  __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_W(fmt, ...) log_message(LOG_WARN,  __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_E(fmt, ...) log_message(LOG_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
