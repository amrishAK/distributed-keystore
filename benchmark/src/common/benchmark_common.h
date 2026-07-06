#ifndef BENCHMARK_COMMON_H
#define BENCHMARK_COMMON_H

#include <inttypes.h>
#include <stddef.h>
#include <time.h>

#include "type_definitions/custom_type_definitions.h"

uint64_t ns_diff(const struct timespec *start, const struct timespec *end);
unsigned long read_peak_rss_kb(void);
unsigned long read_current_rss_kb(void);
int compare_uint64(const void *left_ptr, const void *right_ptr);
uint64_t percentile(const uint64_t *sorted, size_t count, double fraction);
void free_kv_pair(key_value_pair *pair);
void print_benchmark_usage(const char *program_name);

#endif
