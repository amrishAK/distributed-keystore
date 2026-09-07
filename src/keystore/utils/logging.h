#pragma once

#include <stdio.h>

/**
 * @file logging.h
 * @brief Structured logging abstraction for KeyStore
 *
 * Provides compile-time configurable logging macros with level-based filtering:
 * - LOG_DEBUG:  emitted if LOG_LEVEL >= LOG_LEVEL_DEBUG (3)
 * - LOG_INFO:   emitted if LOG_LEVEL >= LOG_LEVEL_INFO (2)
 * - LOG_ERROR:  emitted if LOG_LEVEL >= LOG_LEVEL_ERROR (1)
 *
 * Default LOG_LEVEL is LOG_LEVEL_ERROR (production). To enable additional
 * logging, define LOG_LEVEL before including this header:
 *   #define LOG_LEVEL LOG_LEVEL_DEBUG   // Enable all messages
 *   #define LOG_LEVEL LOG_LEVEL_INFO    // Enable info and error
 *   #define LOG_LEVEL LOG_LEVEL_SILENT  // Disable all logging
 *
 * COMPILER REQUIREMENT: Variadic macro support (GCC-style ##__VA_ARGS__).
 * Supported by: GCC 2.95+, Clang, MSVC 2015+. Some older or non-GCC
 * toolchains may not support this syntax.
 *
 * Usage:
 *   LOG_DEBUG("msg: %d", value);  // Only logged if LOG_LEVEL >= LOG_DEBUG
 *   LOG_ERROR("error: %s", msg);  // Logged if LOG_LEVEL >= LOG_ERROR
 */

/* Log levels */
#define LOG_LEVEL_DEBUG   3
#define LOG_LEVEL_INFO    2
#define LOG_LEVEL_ERROR   1
#define LOG_LEVEL_SILENT  0

/* Default log level: ERROR only (production) */
#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_ERROR
#endif

/* Logging macros */
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
#define LOG_DEBUG(fmt, ...) \
    do { \
        fprintf(stderr, "[DEBUG] " fmt "\n", ##__VA_ARGS__); \
    } while(0)
#else
#define LOG_DEBUG(fmt, ...) ((void)0)
#endif

#if LOG_LEVEL >= LOG_LEVEL_INFO
#define LOG_INFO(fmt, ...) \
    do { \
        fprintf(stderr, "[INFO] " fmt "\n", ##__VA_ARGS__); \
    } while(0)
#else
#define LOG_INFO(fmt, ...) ((void)0)
#endif

#if LOG_LEVEL >= LOG_LEVEL_ERROR
#define LOG_ERROR(fmt, ...) \
    do { \
        fprintf(stderr, "[ERROR] " fmt "\n", ##__VA_ARGS__); \
    } while(0)
#else
#define LOG_ERROR(fmt, ...) ((void)0)
#endif
