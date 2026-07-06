#include "multi_thread_models.h"

#include <string.h>

static const hash_table_configuration g_mt_config = {
    .bucket_size = 1024,
    .is_concurrency_enabled = true,
    .sub_hash_table_bucket_size = 1024,
    .max_linked_list_chain_length = 15
};

static const hash_table_configuration g_mt_resize_config = {
    .bucket_size = 1024,
    .is_concurrency_enabled = true,
    .sub_hash_table_bucket_size = 1024,
    .max_linked_list_chain_length = 2
};

#define MT_SCENARIO(ID, TITLE, FAMILY, WORKLOAD, KIND, PREALLOC, VARIANT, DIST, THREADS, KEYSPACE, VALUE_SIZE, LOW_ENTROPY, TIMELINE, LATENCY, SET_RATIO, GET_RATIO, DELETE_RATIO) \
    {                                                                                                                                                                              \
        ID, TITLE, FAMILY, WORKLOAD, KIND, g_mt_config, PREALLOC, 1.0, 3.0, 5, 250, VARIANT, DIST, THREADS, KEYSPACE, VALUE_SIZE, LOW_ENTROPY, TIMELINE, LATENCY, SET_RATIO, \
            GET_RATIO, DELETE_RATIO                                                                                                                                                 \
    }

#define MT_RESIZE_SCENARIO(ID, TITLE, FAMILY, WORKLOAD, KIND, PREALLOC, VARIANT, DIST, THREADS, KEYSPACE, VALUE_SIZE, LOW_ENTROPY, TIMELINE, LATENCY, SET_RATIO, GET_RATIO, DELETE_RATIO) \
    {                                                                                                                                                                                     \
        ID, TITLE, FAMILY, WORKLOAD, KIND, g_mt_resize_config, PREALLOC, 1.0, 3.0, 5, 250, VARIANT, DIST, THREADS, KEYSPACE, VALUE_SIZE, LOW_ENTROPY, TIMELINE, LATENCY, SET_RATIO, \
            GET_RATIO, DELETE_RATIO                                                                                                                                                        \
    }

static const mt_scenario g_scenarios[] = {
    MT_SCENARIO("MT2-CORE-001", "Core scaling 1T balanced cold", "core_scaling", "50/50 SET/GET", MT_SCALE_PAIR, 0.5, MT_VARIANT_COLD, MT_DIST_UNIFORM, 1, 100000, 64, false, false, true, 0.50, 0.50, 0.0),
    MT_SCENARIO("MT2-CORE-002", "Core scaling 2T balanced cold", "core_scaling", "50/50 SET/GET", MT_SCALE_PAIR, 0.5, MT_VARIANT_COLD, MT_DIST_UNIFORM, 2, 100000, 64, false, false, true, 0.50, 0.50, 0.0),
    MT_SCENARIO("MT2-CORE-003", "Core scaling 4T balanced cold", "core_scaling", "50/50 SET/GET", MT_SCALE_PAIR, 0.5, MT_VARIANT_COLD, MT_DIST_UNIFORM, 4, 100000, 64, false, false, true, 0.50, 0.50, 0.0),
    MT_SCENARIO("MT2-CORE-004", "Core scaling 8T balanced cold", "core_scaling", "50/50 SET/GET", MT_SCALE_PAIR, 0.5, MT_VARIANT_COLD, MT_DIST_UNIFORM, 8, 100000, 64, false, false, true, 0.50, 0.50, 0.0),
    MT_SCENARIO("MT2-CORE-005", "Core scaling 16T balanced cold", "core_scaling", "50/50 SET/GET", MT_SCALE_PAIR, 0.5, MT_VARIANT_COLD, MT_DIST_UNIFORM, 16, 100000, 64, false, false, true, 0.50, 0.50, 0.0),
    MT_SCENARIO("MT2-CORE-006", "Core scaling 32T balanced cold", "core_scaling", "50/50 SET/GET", MT_SCALE_PAIR, 0.5, MT_VARIANT_COLD, MT_DIST_UNIFORM, 32, 100000, 64, false, false, true, 0.50, 0.50, 0.0),

    MT_SCENARIO("MT2-CW-001", "Cold balanced 8T", "workload_mix", "50/50 SET/GET", MT_MIXED_READ_WRITE, 0.5, MT_VARIANT_COLD, MT_DIST_UNIFORM, 8, 100000, 64, false, false, true, 0.50, 0.50, 0.0),
    MT_SCENARIO("MT2-CW-002", "Warm balanced 8T", "workload_mix", "50/50 SET/GET", MT_MIXED_READ_WRITE, 0.5, MT_VARIANT_WARM, MT_DIST_UNIFORM, 8, 100000, 64, false, false, true, 0.50, 0.50, 0.0),
    MT_SCENARIO("MT2-CW-003", "Cold read-heavy 8T", "workload_mix", "10/90 SET/GET", MT_MIXED_READ_WRITE, 0.5, MT_VARIANT_COLD, MT_DIST_UNIFORM, 8, 100000, 64, false, false, true, 0.10, 0.90, 0.0),
    MT_SCENARIO("MT2-CW-004", "Warm read-heavy 8T", "workload_mix", "10/90 SET/GET", MT_MIXED_READ_WRITE, 0.5, MT_VARIANT_WARM, MT_DIST_UNIFORM, 8, 100000, 64, false, false, true, 0.10, 0.90, 0.0),
    MT_SCENARIO("MT2-CW-005", "Cold write-heavy 8T", "workload_mix", "90/10 SET/GET", MT_MIXED_READ_WRITE, 0.5, MT_VARIANT_COLD, MT_DIST_UNIFORM, 8, 100000, 64, false, false, true, 0.90, 0.10, 0.0),
    MT_SCENARIO("MT2-CW-006", "Warm write-heavy 8T", "workload_mix", "90/10 SET/GET", MT_MIXED_READ_WRITE, 0.5, MT_VARIANT_WARM, MT_DIST_UNIFORM, 8, 100000, 64, false, false, true, 0.90, 0.10, 0.0),

    MT_SCENARIO("MT2-MIX-001", "Read-only 8T", "workload_mix", "0/100 SET/GET", MT_MIXED_READ_WRITE, 0.5, MT_VARIANT_WARM, MT_DIST_UNIFORM, 8, 100000, 64, false, false, true, 0.0, 1.0, 0.0),
    MT_SCENARIO("MT2-MIX-002", "Write-only 8T", "workload_mix", "100/0 SET/GET", MT_MIXED_READ_WRITE, 0.5, MT_VARIANT_COLD, MT_DIST_UNIFORM, 8, 100000, 64, false, false, true, 1.0, 0.0, 0.0),
    MT_SCENARIO("MT2-MIX-003", "Balanced 8T", "workload_mix", "50/50 SET/GET", MT_MIXED_READ_WRITE, 0.5, MT_VARIANT_WARM, MT_DIST_UNIFORM, 8, 100000, 64, false, false, true, 0.50, 0.50, 0.0),
    MT_SCENARIO("MT2-MIX-004", "Read-heavy 8T", "workload_mix", "10/90 SET/GET", MT_MIXED_READ_WRITE, 0.5, MT_VARIANT_WARM, MT_DIST_UNIFORM, 8, 100000, 64, false, false, true, 0.10, 0.90, 0.0),
    MT_SCENARIO("MT2-MIX-005", "Write-heavy 8T", "workload_mix", "90/10 SET/GET", MT_MIXED_READ_WRITE, 0.5, MT_VARIANT_WARM, MT_DIST_UNIFORM, 8, 100000, 64, false, false, true, 0.90, 0.10, 0.0),
    MT_SCENARIO("MT2-MIX-006", "Read-only 16T", "workload_mix", "0/100 SET/GET", MT_MIXED_READ_WRITE, 0.5, MT_VARIANT_WARM, MT_DIST_UNIFORM, 16, 100000, 64, false, false, true, 0.0, 1.0, 0.0),
    MT_SCENARIO("MT2-MIX-007", "Write-only 16T", "workload_mix", "100/0 SET/GET", MT_MIXED_READ_WRITE, 0.5, MT_VARIANT_COLD, MT_DIST_UNIFORM, 16, 100000, 64, false, false, true, 1.0, 0.0, 0.0),
    MT_SCENARIO("MT2-MIX-008", "Balanced 16T", "workload_mix", "50/50 SET/GET", MT_MIXED_READ_WRITE, 0.5, MT_VARIANT_WARM, MT_DIST_UNIFORM, 16, 100000, 64, false, false, true, 0.50, 0.50, 0.0),
    MT_SCENARIO("MT2-MIX-009", "Read-heavy 16T", "workload_mix", "10/90 SET/GET", MT_MIXED_READ_WRITE, 0.5, MT_VARIANT_WARM, MT_DIST_UNIFORM, 16, 100000, 64, false, false, true, 0.10, 0.90, 0.0),
    MT_SCENARIO("MT2-MIX-010", "Write-heavy 16T", "workload_mix", "90/10 SET/GET", MT_MIXED_READ_WRITE, 0.5, MT_VARIANT_WARM, MT_DIST_UNIFORM, 16, 100000, 64, false, false, true, 0.90, 0.10, 0.0),

    MT_SCENARIO("MT2-DIST-001", "Balanced 8T uniform", "workload_mix", "50/50 SET/GET", MT_MIXED_READ_WRITE, 0.5, MT_VARIANT_WARM, MT_DIST_UNIFORM, 8, 100000, 64, false, false, true, 0.50, 0.50, 0.0),
    MT_SCENARIO("MT2-DIST-002", "Balanced 8T zipf", "workload_mix", "50/50 SET/GET", MT_MIXED_READ_WRITE, 0.5, MT_VARIANT_WARM, MT_DIST_ZIPF, 8, 100000, 64, false, false, true, 0.50, 0.50, 0.0),
    MT_SCENARIO("MT2-DIST-003", "Balanced 8T bursty", "workload_mix", "50/50 SET/GET", MT_MIXED_READ_WRITE, 0.5, MT_VARIANT_WARM, MT_DIST_BURSTY, 8, 100000, 64, false, false, true, 0.50, 0.50, 0.0),
    MT_SCENARIO("MT2-DIST-004", "Read-heavy 16T uniform", "workload_mix", "10/90 SET/GET", MT_MIXED_READ_WRITE, 0.5, MT_VARIANT_WARM, MT_DIST_UNIFORM, 16, 100000, 64, false, false, true, 0.10, 0.90, 0.0),
    MT_SCENARIO("MT2-DIST-005", "Read-heavy 16T zipf", "workload_mix", "10/90 SET/GET", MT_MIXED_READ_WRITE, 0.5, MT_VARIANT_WARM, MT_DIST_ZIPF, 16, 100000, 64, false, false, true, 0.10, 0.90, 0.0),
    MT_SCENARIO("MT2-DIST-006", "Read-heavy 16T bursty", "workload_mix", "10/90 SET/GET", MT_MIXED_READ_WRITE, 0.5, MT_VARIANT_WARM, MT_DIST_BURSTY, 16, 100000, 64, false, false, true, 0.10, 0.90, 0.0),

    MT_RESIZE_SCENARIO("MT2-RSZ-001", "Resize stress 8T prealloc=0.0 uniform", "resize_stress", "write-heavy", MT_RESIZE_WRITE, 0.0, MT_VARIANT_COLD, MT_DIST_UNIFORM, 8, 100000, 64, false, true, true, 1.0, 0.0, 0.0),
    MT_RESIZE_SCENARIO("MT2-RSZ-002", "Resize stress 8T prealloc=0.5 uniform", "resize_stress", "write-heavy", MT_RESIZE_WRITE, 0.5, MT_VARIANT_COLD, MT_DIST_UNIFORM, 8, 100000, 64, false, true, true, 1.0, 0.0, 0.0),
    MT_RESIZE_SCENARIO("MT2-RSZ-003", "Resize stress 8T prealloc=1.0 uniform", "resize_stress", "write-heavy", MT_RESIZE_WRITE, 1.0, MT_VARIANT_COLD, MT_DIST_UNIFORM, 8, 100000, 64, false, true, true, 1.0, 0.0, 0.0),
    MT_RESIZE_SCENARIO("MT2-RSZ-004", "Resize stress 16T prealloc=0.0 uniform", "resize_stress", "write-heavy", MT_RESIZE_WRITE, 0.0, MT_VARIANT_COLD, MT_DIST_UNIFORM, 16, 100000, 64, false, true, true, 1.0, 0.0, 0.0),
    MT_RESIZE_SCENARIO("MT2-RSZ-005", "Resize stress 16T prealloc=0.5 uniform", "resize_stress", "write-heavy", MT_RESIZE_WRITE, 0.5, MT_VARIANT_COLD, MT_DIST_UNIFORM, 16, 100000, 64, false, true, true, 1.0, 0.0, 0.0),
    MT_RESIZE_SCENARIO("MT2-RSZ-006", "Resize stress 16T prealloc=1.0 uniform", "resize_stress", "write-heavy", MT_RESIZE_WRITE, 1.0, MT_VARIANT_COLD, MT_DIST_UNIFORM, 16, 100000, 64, false, true, true, 1.0, 0.0, 0.0),

    MT_SCENARIO("MT2-VKS-001", "Value/keyspace 8T 64B 100K balanced", "workload_mix", "50/50 SET/GET", MT_MIXED_READ_WRITE, 0.5, MT_VARIANT_WARM, MT_DIST_UNIFORM, 8, 100000, 64, false, false, true, 0.50, 0.50, 0.0),
    MT_SCENARIO("MT2-VKS-002", "Value/keyspace 8T 64B 1M read-heavy", "workload_mix", "10/90 SET/GET", MT_MIXED_READ_WRITE, 0.5, MT_VARIANT_WARM, MT_DIST_UNIFORM, 8, 1000000, 64, false, false, true, 0.10, 0.90, 0.0),
    MT_SCENARIO("MT2-VKS-003", "Value/keyspace 8T 1KB 100K balanced", "workload_mix", "50/50 SET/GET", MT_MIXED_READ_WRITE, 0.5, MT_VARIANT_WARM, MT_DIST_UNIFORM, 8, 100000, 1024, false, false, true, 0.50, 0.50, 0.0),
    MT_SCENARIO("MT2-VKS-004", "Value/keyspace 8T 1KB 1M read-heavy", "workload_mix", "10/90 SET/GET", MT_MIXED_READ_WRITE, 0.5, MT_VARIANT_WARM, MT_DIST_UNIFORM, 8, 1000000, 1024, false, false, true, 0.10, 0.90, 0.0),
    MT_SCENARIO("MT2-VKS-005", "Value/keyspace 16T 64B 100K balanced", "workload_mix", "50/50 SET/GET", MT_MIXED_READ_WRITE, 0.5, MT_VARIANT_WARM, MT_DIST_UNIFORM, 16, 100000, 64, false, false, true, 0.50, 0.50, 0.0),
    MT_SCENARIO("MT2-VKS-006", "Value/keyspace 16T 64B 1M read-heavy", "workload_mix", "10/90 SET/GET", MT_MIXED_READ_WRITE, 0.5, MT_VARIANT_WARM, MT_DIST_UNIFORM, 16, 1000000, 64, false, false, true, 0.10, 0.90, 0.0),
    MT_SCENARIO("MT2-VKS-007", "Value/keyspace 16T 1KB 100K balanced", "workload_mix", "50/50 SET/GET", MT_MIXED_READ_WRITE, 0.5, MT_VARIANT_WARM, MT_DIST_UNIFORM, 16, 100000, 1024, false, false, true, 0.50, 0.50, 0.0),
    MT_SCENARIO("MT2-VKS-008", "Value/keyspace 16T 1KB 1M read-heavy", "workload_mix", "10/90 SET/GET", MT_MIXED_READ_WRITE, 0.5, MT_VARIANT_WARM, MT_DIST_UNIFORM, 16, 1000000, 1024, false, false, true, 0.10, 0.90, 0.0),

    MT_SCENARIO("MT2-OVER-001", "Oversubscription 64T balanced", "oversubscription", "50/50 SET/GET", MT_SCALE_PAIR, 0.5, MT_VARIANT_COLD, MT_DIST_UNIFORM, 64, 100000, 64, false, false, true, 0.50, 0.50, 0.0),
    MT_SCENARIO("MT2-OVER-002", "Oversubscription 128T balanced", "oversubscription", "50/50 SET/GET", MT_SCALE_PAIR, 0.5, MT_VARIANT_COLD, MT_DIST_UNIFORM, 128, 100000, 64, false, false, true, 0.50, 0.50, 0.0),
    MT_SCENARIO("MT2-OVER-003", "Oversubscription 2000T balanced", "oversubscription", "50/50 SET/GET", MT_SCALE_PAIR, 0.5, MT_VARIANT_COLD, MT_DIST_UNIFORM, 2000, 100000, 64, false, false, true, 0.50, 0.50, 0.0)
};

#undef MT_SCENARIO
#undef MT_RESIZE_SCENARIO

const mt_scenario *
multi_thread_scenarios(size_t *count)
{
    size_t scenario_count = sizeof(g_scenarios) / sizeof(g_scenarios[0]);
    if (count != NULL)
    {
        *count = scenario_count;
    }
    return g_scenarios;
}

const mt_scenario *
multi_thread_find_scenario(const char *test_id)
{
    size_t scenario_count = 0;
    const mt_scenario *scenarios = multi_thread_scenarios(&scenario_count);

    for (size_t index = 0; index < scenario_count; ++index)
    {
        if (strcmp(test_id, scenarios[index].test_id) == 0)
        {
            return &scenarios[index];
        }
    }

    return NULL;
}
