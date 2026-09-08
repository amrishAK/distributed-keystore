
#include "helper_functions.h"
#include "type_definitions/error_code_definitions.h"

#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

// A constant for deriving distinct seeds, using the golden ratio to ensure good distribution
#define GOLDEN_RATIO_CONSTANT 0x9e3779b97f4a7c15ULL

#pragma region Public Function Definitions

int is_power_of_two(unsigned int bucket_size) {
    return (bucket_size != 0) && ((bucket_size & (bucket_size - 1)) == 0);
}


void portable_sleep_ms(unsigned long ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    portable_sleep_us(ms * 1000UL);
#endif
}

void portable_sleep_us(unsigned long microseconds) {
#ifdef _WIN32
    unsigned long milliseconds = (microseconds + 999UL) / 1000UL;
    Sleep(milliseconds);
#else
    struct timespec delay = {
        .tv_sec = (time_t)(microseconds / 1000000UL),
        .tv_nsec = (long)((microseconds % 1000000UL) * 1000UL)
    };
    nanosleep(&delay, NULL);
#endif
}

/**
 * @fn generate_hash_seed
 * @brief Generates a hash seed using system time.
 * 
 * For windows:
 *   Uses GetSystemTimeAsFileTime to get the current time in 100-nanosecond intervals since January 1, 1601 (UTC). 
 *   This provides a high-resolution timestamp suitable for generating a hash seed.
 * 
 * For POSIX systems:
 *  Uses clock_gettime with CLOCK_MONOTONIC to get the current time in seconds and nanoseconds since an unspecified starting point (monotonic time). 
 *  This provides a high-resolution timestamp that is not affected by changes in the system clock, making it suitable for generating a hash seed.
 * 
 * @return A 64-bit unsigned integer seed derived from the current system time, providing a unique seed for hash functions.
 * 
 */
uint64_t generate_hash_seed()
{
    #ifdef _WIN32
        // Windows: Use GetSystemTimeAsFileTime for nanosecond-ish resolution
        FILETIME ft;
        GetSystemTimeAsFileTime(&ft);
        // FILETIME is 100-nanosecond intervals since Jan 1, 1601 (UTC)
        ULARGE_INTEGER uli;
        uli.LowPart = ft.dwLowDateTime;
        uli.HighPart = ft.dwHighDateTime;
        return (uint64_t)uli.QuadPart;
    #else
        // POSIX: Use clock_gettime for nanosecond resolution
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
    #endif
}


uint64_t derive_distinct_seed(uint64_t base_seed) {
    // GOLDEN_RATIO_CONSTANT is good for hash mixing
    return base_seed ^ GOLDEN_RATIO_CONSTANT;
}

#pragma endregion

