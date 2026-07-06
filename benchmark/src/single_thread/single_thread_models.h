#ifndef SINGLE_THREAD_MODELS_H
#define SINGLE_THREAD_MODELS_H

#include <stdbool.h>
#include <stddef.h>

#include "type_definitions/config_type_definitions.h"
#include "type_definitions/custom_type_definitions.h"

typedef enum
{
    BENCH_SET_NEW,
    BENCH_SET_UPDATE,
    BENCH_GET_HIT,
    BENCH_GET_MISS,
    BENCH_DELETE,
    BENCH_MIXED
} bench_kind;

typedef enum
{
    BENCH_VARIANT_COLD,
    BENCH_VARIANT_WARM
} bench_variant;

typedef enum
{
    BENCH_DIST_UNIFORM,
    BENCH_DIST_ZIPF,
    BENCH_DIST_BURSTY
} bench_distribution;

typedef struct
{
    const char *test_id;
    const char *title;
    const char *category;
    bench_kind kind;
    hash_table_configuration config;
    double pre_allocation_factor;
    double warmup_seconds;
    double measure_seconds;
    unsigned int repetitions;
    unsigned int cooldown_ms;
    bench_variant variant;
    bench_distribution distribution;
    size_t keyspace_size;
    size_t value_size;
    bool low_entropy_keys;
    bool collect_resize_timeline;
    bool measure_latency;
    double set_ratio;
    double get_ratio;
    double delete_ratio;
} bench_scenario;

const bench_scenario *single_thread_scenarios(size_t *count);
const bench_scenario *single_thread_find_scenario(const char *test_id);

#endif
