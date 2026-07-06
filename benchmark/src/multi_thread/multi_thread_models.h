#ifndef MULTI_THREAD_MODELS_H
#define MULTI_THREAD_MODELS_H

#include <stdbool.h>
#include <stddef.h>

#include "type_definitions/config_type_definitions.h"
#include "type_definitions/custom_type_definitions.h"

typedef enum
{
    MT_SCALE_PAIR,
    MT_MIXED_READ_WRITE,
    MT_RESIZE_WRITE,
    MT_SOFT_DELETE
} mt_workload_kind;

typedef enum
{
    MT_VARIANT_COLD,
    MT_VARIANT_WARM
} mt_variant;

typedef enum
{
    MT_DIST_UNIFORM,
    MT_DIST_ZIPF,
    MT_DIST_BURSTY
} mt_distribution;

typedef struct
{
    const char *test_id;
    const char *title;
    const char *family;
    const char *workload;
    mt_workload_kind kind;
    hash_table_configuration config;
    double pre_allocation_factor;
    double warmup_seconds;
    double measure_seconds;
    unsigned int repetitions;
    unsigned int cooldown_ms;
    mt_variant variant;
    mt_distribution distribution;
    unsigned int threads;
    size_t keyspace_size;
    size_t value_size;
    bool low_entropy_keys;
    bool collect_resize_timeline;
    bool measure_latency;
    double set_ratio;
    double get_ratio;
    double delete_ratio;
} mt_scenario;

const mt_scenario *multi_thread_scenarios(size_t *count);
const mt_scenario *multi_thread_find_scenario(const char *test_id);

#endif
