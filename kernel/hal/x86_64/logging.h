#pragma once

#include "serial.h"

#include <le/formatter.h>
#include <utility>

#define log_info(fmt, ...) { \
    serial::debug_write_string("[info] " __FILE__ ": "); \
    char buffer[128]; \
    format_n(buffer, 128, fmt __VA_OPT__(,) __VA_ARGS__); \
    serial::debug_write_string(buffer); \
    serial::debug_write_string("\r\n"); \
}

#define log_error(fmt, ...) { \
    serial::debug_write_string("[error] " __FILE__ ": "); \
    char buffer[128]; \
    format_n(buffer, 128, fmt __VA_OPT__(,) __VA_ARGS__); \
    serial::debug_write_string(buffer); \
    serial::debug_write_string("\r\n"); \
}

#define log_fatal(fmt, ...) { \
    serial::debug_write_string("[fatal] " __FILE__ ": "); \
    char buffer[128]; \
    format_n(buffer, 128, fmt __VA_OPT__(,) __VA_ARGS__); \
    serial::debug_write_string(buffer); \
    serial::debug_write_string("\r\n"); \
    __builtin_unreachable(); \
}
