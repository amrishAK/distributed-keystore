#include "single_thread_models.h"

#include <string.h>

static const hash_table_configuration g_baseline_config = {
    .bucket_size = 256,
    .is_concurrency_enabled = false,
    .sub_hash_table_bucket_size = 32,
    .max_linked_list_chain_length = 8
};

static const hash_table_configuration g_resize_stress_config = {
    .bucket_size = 256,
    .is_concurrency_enabled = false,
    .sub_hash_table_bucket_size = 32,
    .max_linked_list_chain_length = 2
};

static const bench_scenario g_scenarios[] = {
    {"ST2-CORE-001", "50/50 SET/GET warm uniform", "realistic", BENCH_MIXED, g_baseline_config, 0.5, 1.0, 3.0, 5, 250, BENCH_VARIANT_WARM, BENCH_DIST_UNIFORM, 10000, 64, false, false, true, 0.50, 0.50, 0.0},
    {"ST2-CORE-002", "10/90 SET/GET warm uniform", "realistic", BENCH_MIXED, g_baseline_config, 0.5, 1.0, 3.0, 5, 250, BENCH_VARIANT_WARM, BENCH_DIST_UNIFORM, 10000, 64, false, false, true, 0.10, 0.90, 0.0},
    {"ST2-CORE-003", "90/10 SET/GET warm uniform", "realistic", BENCH_MIXED, g_baseline_config, 0.5, 1.0, 3.0, 5, 250, BENCH_VARIANT_WARM, BENCH_DIST_UNIFORM, 10000, 64, false, false, true, 0.90, 0.10, 0.0},
    {"ST2-CORE-004", "100% GET hit warm uniform", "micro", BENCH_GET_HIT, g_baseline_config, 0.5, 1.0, 3.0, 5, 250, BENCH_VARIANT_WARM, BENCH_DIST_UNIFORM, 10000, 64, false, false, true, 0.0, 1.0, 0.0},
    {"ST2-CORE-005", "100% SET update warm uniform", "micro", BENCH_SET_UPDATE, g_baseline_config, 0.5, 1.0, 3.0, 5, 250, BENCH_VARIANT_WARM, BENCH_DIST_UNIFORM, 100000, 64, false, false, true, 1.0, 0.0, 0.0},

    {"ST2-CW-001", "50/50 SET/GET cold uniform", "realistic", BENCH_MIXED, g_baseline_config, 0.5, 1.0, 3.0, 5, 250, BENCH_VARIANT_COLD, BENCH_DIST_UNIFORM, 10000, 64, false, false, true, 0.50, 0.50, 0.0},
    {"ST2-CW-002", "50/50 SET/GET warm uniform", "realistic", BENCH_MIXED, g_baseline_config, 0.5, 1.0, 3.0, 5, 250, BENCH_VARIANT_WARM, BENCH_DIST_UNIFORM, 10000, 64, false, false, true, 0.50, 0.50, 0.0},

    {"ST2-DIST-001", "Uniform random distribution", "realistic", BENCH_MIXED, g_baseline_config, 0.5, 1.0, 3.0, 5, 250, BENCH_VARIANT_WARM, BENCH_DIST_UNIFORM, 10000, 64, false, false, true, 0.50, 0.50, 0.0},
    {"ST2-DIST-002", "Zipf distribution theta=0.99", "realistic", BENCH_MIXED, g_baseline_config, 0.5, 1.0, 3.0, 5, 250, BENCH_VARIANT_WARM, BENCH_DIST_ZIPF, 10000, 64, false, false, true, 0.50, 0.50, 0.0},
    {"ST2-DIST-003", "Bursty 80/20 temporal locality", "realistic", BENCH_MIXED, g_baseline_config, 0.5, 1.0, 3.0, 5, 250, BENCH_VARIANT_WARM, BENCH_DIST_BURSTY, 10000, 64, false, false, true, 0.50, 0.50, 0.0},

    {"ST2-VKS-001", "VKS warm value=64B keyspace=10K", "realistic", BENCH_MIXED, g_baseline_config, 0.5, 1.0, 3.0, 5, 250, BENCH_VARIANT_WARM, BENCH_DIST_UNIFORM, 10000, 64, false, false, true, 0.50, 0.50, 0.0},
    {"ST2-VKS-002", "VKS warm value=64B keyspace=20K", "realistic", BENCH_MIXED, g_baseline_config, 0.5, 1.0, 3.0, 5, 250, BENCH_VARIANT_WARM, BENCH_DIST_UNIFORM, 20000, 64, false, false, true, 0.50, 0.50, 0.0},
    {"ST2-VKS-003", "VKS warm value=1KB keyspace=10K", "realistic", BENCH_MIXED, g_baseline_config, 0.5, 1.0, 3.0, 5, 250, BENCH_VARIANT_WARM, BENCH_DIST_UNIFORM, 10000, 1024, false, false, true, 0.50, 0.50, 0.0},
    {"ST2-VKS-004", "VKS warm value=1KB keyspace=20K", "realistic", BENCH_MIXED, g_baseline_config, 0.5, 1.0, 3.0, 5, 250, BENCH_VARIANT_WARM, BENCH_DIST_UNIFORM, 20000, 1024, false, false, true, 0.50, 0.50, 0.0},

    {"ST2-RSZ-001", "Resize stress prealloc=0.0", "resize", BENCH_SET_NEW, g_resize_stress_config, 0.0, 1.0, 3.0, 5, 250, BENCH_VARIANT_COLD, BENCH_DIST_UNIFORM, 20000, 64, false, true, true, 1.0, 0.0, 0.0},
    {"ST2-RSZ-002", "Resize stress prealloc=0.5", "resize", BENCH_SET_NEW, g_resize_stress_config, 0.5, 1.0, 3.0, 5, 250, BENCH_VARIANT_COLD, BENCH_DIST_UNIFORM, 20000, 64, false, true, true, 1.0, 0.0, 0.0},
    {"ST2-RSZ-003", "Resize stress prealloc=1.0", "resize", BENCH_SET_NEW, g_resize_stress_config, 1.0, 1.0, 3.0, 5, 250, BENCH_VARIANT_COLD, BENCH_DIST_UNIFORM, 20000, 64, false, true, true, 1.0, 0.0, 0.0},

    {"ST2-MEM-001", "Memory diagnostics prealloc=0.0", "memory", BENCH_MIXED, g_baseline_config, 0.0, 1.0, 3.0, 5, 250, BENCH_VARIANT_WARM, BENCH_DIST_UNIFORM, 10000, 64, false, false, true, 0.50, 0.50, 0.0},
    {"ST2-MEM-002", "Memory diagnostics prealloc=0.5", "memory", BENCH_MIXED, g_baseline_config, 0.5, 1.0, 3.0, 5, 250, BENCH_VARIANT_WARM, BENCH_DIST_UNIFORM, 10000, 64, false, false, true, 0.50, 0.50, 0.0},
    {"ST2-MEM-003", "Memory diagnostics prealloc=1.0", "memory", BENCH_MIXED, g_baseline_config, 1.0, 1.0, 3.0, 5, 250, BENCH_VARIANT_WARM, BENCH_DIST_UNIFORM, 10000, 64, false, false, true, 0.50, 0.50, 0.0}
};

const bench_scenario *
single_thread_scenarios(size_t *count)
{
    size_t scenario_count = sizeof(g_scenarios) / sizeof(g_scenarios[0]);
    if (count != NULL)
    {
        *count = scenario_count;
    }
    return g_scenarios;
}

const bench_scenario *
single_thread_find_scenario(const char *test_id)
{
    size_t scenario_count = 0;
    const bench_scenario *scenarios = single_thread_scenarios(&scenario_count);

    for (size_t index = 0; index < scenario_count; ++index)
    {
        if (strcmp(test_id, scenarios[index].test_id) == 0)
        {
            return &scenarios[index];
        }
    }

    return NULL;
}
