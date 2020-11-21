#include <stdarg.h>
#include <stdint.h>
#include <assert.h>

#include "log.h"

#define FFI_LOG_CODE(...) \
    va_list args; \
    va_start(args, format); \
    \
    ffi_log(__VA_ARGS__); \
    \
    va_end(args);

// This function is implemented in Rust
void ffi_log(const char *level, const char *target, const char *format, void *args);

void log(const char *level, const char *target, const char *format, ...) {
    FFI_LOG_CODE(level, target, format, args)
}

void trace(const char *target, const char *format, ...) {
    FFI_LOG_CODE("TRACE", target, format, args)
}

void debug(const char *target, const char *format, ...) {
    FFI_LOG_CODE("DEBUG", target, format, args)
}

void info(const char *target, const char *format, ...) {
    FFI_LOG_CODE("INFO", target, format, args)
}

void warn(const char *target, const char *format, ...) {
    FFI_LOG_CODE("WARN", target, format, args)
}

void error(const char *target, const char *format, ...) {
    FFI_LOG_CODE("ERROR", target, format, args)
}