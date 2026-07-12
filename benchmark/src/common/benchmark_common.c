#define _POSIX_C_SOURCE 200809L

#include "benchmark_common.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#include <unistd.h>
#endif

uint64_t
ns_diff(const struct timespec *start, const struct timespec *end)
{
    return (uint64_t)(end->tv_sec - start->tv_sec) * 1000000000ULL + (uint64_t)(end->tv_nsec - start->tv_nsec);
}

unsigned long
read_peak_rss_kb(void)
{
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS counters;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)) != 0)
    {
        return (unsigned long)(counters.PeakWorkingSetSize / 1024UL);
    }
    return 0;
#else
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0)
    {
        return (unsigned long)usage.ru_maxrss;
    }
    return 0;
#endif
}

unsigned long
read_current_rss_kb(void)
{
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS counters;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)) != 0)
    {
        return (unsigned long)(counters.WorkingSetSize / 1024UL);
    }
    return 0;
#else
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0)
    {
        return 0;
    }

    FILE *file = fopen("/proc/self/statm", "r");
    if (file == NULL)
    {
        return 0;
    }

    unsigned long total_pages = 0;
    unsigned long resident_pages = 0;
    if (fscanf(file, "%lu %lu", &total_pages, &resident_pages) != 2)
    {
        fclose(file);
        return 0;
    }
    fclose(file);

    return (unsigned long)((resident_pages * (unsigned long)page_size) / 1024UL);
#endif
}

int
compare_uint64(const void *left_ptr, const void *right_ptr)
{
    const uint64_t left = *(const uint64_t *)left_ptr;
    const uint64_t right = *(const uint64_t *)right_ptr;
    if (left < right)
    {
        return -1;
    }
    if (left > right)
    {
        return 1;
    }
    return 0;
}

uint64_t
percentile(const uint64_t *sorted, size_t count, double fraction)
{
    if (count == 0)
    {
        return 0;
    }

    size_t index = (size_t)((double)(count - 1) * fraction);
    if (index >= count)
    {
        index = count - 1;
    }

    return sorted[index];
}

void
free_kv_pair(key_value_pair *pair)
{
    if (pair == NULL)
    {
        return;
    }

    free(pair->key);
    free(pair->value);
    pair->key = NULL;
    pair->value = NULL;
    pair->value_size = 0;
}

void
print_benchmark_usage(const char *program_name)
{
    printf("Usage: %s [--scenario TEST_ID|all] [--scenario-prefix PREFIX] [--results-file path]\n", program_name);
}
