#define _POSIX_C_SOURCE 200809L

#include "single_thread_runner.h"

#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "benchmark_common.h"
#include "core/key_store.h"
#include "type_definitions/error_code_definitions.h"
#include "type_definitions/sucess_code_definitions.h"

typedef struct
{
    char *key;
} key_slot;

typedef struct
{
    uint64_t *values;
    size_t count;
    size_t capacity;
    size_t max_samples;
} latency_store;

typedef struct
{
    uint64_t p99_ns;
    unsigned int resize_delta;
    unsigned int max_chain_depth;
    size_t operations;
} timeline_slice;

typedef struct
{
    timeline_slice *slices;
    size_t count;
    size_t capacity;
} timeline_store;

typedef struct
{
    uint64_t state;
} prng_state;

typedef struct
{
    unsigned long success_count;
    unsigned long failure_count;
    unsigned long missing_count;
    unsigned int resize_count;
} run_counters;

static bool is_set_success(int result)
{
    return result == SUCCESS ||
           result == SUCESS_ADDED_NEW_NODE ||
           result == SUCCESS_ADDED_TO_PENDING_LIST ||
           result == SUCESS_ADDED_NEW_NODE_RESZING_TRIGGERED;
}

static char *copy_string(const char *text)
{
    size_t length = strlen(text) + 1;
    char *copy = (char *)malloc(length);
    if (copy == NULL)
    {
        return NULL;
    }
    memcpy(copy, text, length);
    return copy;
}

static uint64_t hash_string(const char *text)
{
    uint64_t hash = 1469598103934665603ULL;
    while (*text != '\0')
    {
        hash ^= (uint64_t)(unsigned char)(*text);
        hash *= 1099511628211ULL;
        ++text;
    }
    return hash;
}

static uint64_t prng_next(prng_state *rng)
{
    uint64_t x = rng->state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    rng->state = x;
    return x * 2685821657736338717ULL;
}

static double prng_uniform(prng_state *rng)
{
    return (double)(prng_next(rng) >> 11) * (1.0 / 9007199254740992.0);
}

static key_slot *create_key_slots(const char *prefix, size_t count, bool low_entropy)
{
    key_slot *slots = (key_slot *)calloc(count, sizeof(key_slot));
    if (slots == NULL)
    {
        return NULL;
    }

    size_t entropy_mod = low_entropy ? 256 : count;
    if (entropy_mod == 0)
    {
        entropy_mod = 1;
    }

    for (size_t index = 0; index < count; ++index)
    {
        size_t key_id = low_entropy ? (index % entropy_mod) : index;
        char buffer[96];
        int written = snprintf(buffer, sizeof(buffer), "%s_%08zu", prefix, key_id);
        if (written < 0 || (size_t)written >= sizeof(buffer))
        {
            for (size_t i = 0; i < index; ++i)
            {
                free(slots[i].key);
            }
            free(slots);
            return NULL;
        }

        slots[index].key = copy_string(buffer);
        if (slots[index].key == NULL)
        {
            for (size_t i = 0; i < index; ++i)
            {
                free(slots[i].key);
            }
            free(slots);
            return NULL;
        }
    }

    return slots;
}

static void free_key_slots(key_slot *slots, size_t count)
{
    if (slots == NULL)
    {
        return;
    }

    for (size_t index = 0; index < count; ++index)
    {
        free(slots[index].key);
    }

    free(slots);
}

static unsigned char *create_value_buffer(size_t value_size, unsigned char seed)
{
    unsigned char *value = (unsigned char *)malloc(value_size);
    if (value == NULL)
    {
        return NULL;
    }

    for (size_t index = 0; index < value_size; ++index)
    {
        value[index] = (unsigned char)(seed + (unsigned char)(index & 0x7FU));
    }

    return value;
}

static bool latency_store_append(latency_store *store, uint64_t sample)
{
    if (store->max_samples > 0 && store->count >= store->max_samples)
    {
        return true;
    }

    if (store->count >= store->capacity)
    {
        size_t new_capacity = store->capacity == 0 ? 4096 : (store->capacity * 2);
        if (store->max_samples > 0 && new_capacity > store->max_samples)
        {
            new_capacity = store->max_samples;
        }
        uint64_t *new_values = (uint64_t *)realloc(store->values, new_capacity * sizeof(uint64_t));
        if (new_values == NULL)
        {
            return false;
        }
        store->values = new_values;
        store->capacity = new_capacity;
    }

    store->values[store->count++] = sample;
    return true;
}

static bool timeline_store_append(timeline_store *store, timeline_slice slice)
{
    if (store->count >= store->capacity)
    {
        size_t new_capacity = store->capacity == 0 ? 16 : (store->capacity * 2);
        timeline_slice *new_slices = (timeline_slice *)realloc(store->slices, new_capacity * sizeof(timeline_slice));
        if (new_slices == NULL)
        {
            return false;
        }
        store->slices = new_slices;
        store->capacity = new_capacity;
    }

    store->slices[store->count++] = slice;
    return true;
}

static size_t sample_key_index(const bench_scenario *scenario, prng_state *rng, size_t op_index)
{
    size_t keyspace = scenario->keyspace_size == 0 ? 1 : scenario->keyspace_size;

    if (scenario->distribution == BENCH_DIST_ZIPF)
    {
        const double theta = 0.99;
        double u = prng_uniform(rng);
        if (u < 0.0)
        {
            u = 0.0;
        }
        if (u > 0.999999999)
        {
            u = 0.999999999;
        }

        double exp_component = 1.0 / (1.0 - theta);
        double rank = pow(u, exp_component);
        size_t index = (size_t)(rank * (double)keyspace);
        if (index >= keyspace)
        {
            index = keyspace - 1;
        }
        return index;
    }

    if (scenario->distribution == BENCH_DIST_BURSTY)
    {
        size_t window = keyspace / 5;
        if (window == 0)
        {
            window = 1;
        }

        if ((prng_next(rng) % 100U) < 80U)
        {
            size_t base = op_index % keyspace;
            return (base + (size_t)(prng_next(rng) % window)) % keyspace;
        }

        return (size_t)(prng_next(rng) % keyspace);
    }

    return (size_t)(prng_next(rng) % keyspace);
}

static int set_one(const key_slot *slot, const unsigned char *value, size_t value_size, run_counters *counters)
{
    key_value_pair pair = {
        .key = slot->key,
        .value = (unsigned char *)value,
        .value_size = value_size};

    int result = set_key(&pair);
    if (is_set_success(result))
    {
        ++counters->success_count;
        if (result == SUCESS_ADDED_NEW_NODE_RESZING_TRIGGERED)
        {
            ++counters->resize_count;
        }
    }
    else
    {
        ++counters->failure_count;
        ++counters->missing_count;
    }

    return result;
}

static int get_hit(const key_slot *slot, const unsigned char *expected_value, size_t expected_size, run_counters *counters)
{
    key_value_pair out = {0};
    int result = get_key(slot->key, &out);

    if (result == SUCCESS)
    {
        bool matched = out.key != NULL && out.value != NULL && out.value_size == expected_size && strcmp(out.key, slot->key) == 0 &&
                       memcmp(out.value, expected_value, expected_size) == 0;
        if (matched)
        {
            ++counters->success_count;
        }
        else
        {
            ++counters->failure_count;
            ++counters->missing_count;
        }
    }
    else
    {
        ++counters->failure_count;
        ++counters->missing_count;
    }

    free_kv_pair(&out);
    return result;
}

static int get_miss(const key_slot *slot, run_counters *counters)
{
    key_value_pair out = {0};
    int result = get_key(slot->key, &out);
    free_kv_pair(&out);

    if (result == SUCCESS)
    {
        ++counters->failure_count;
        ++counters->missing_count;
    }
    else
    {
        ++counters->success_count;
    }

    return result;
}

static int delete_one(const key_slot *slot, run_counters *counters)
{
    int result = delete_key(slot->key);
    if (result == SUCCESS)
    {
        ++counters->success_count;
    }
    else
    {
        ++counters->failure_count;
        ++counters->missing_count;
    }

    return result;
}

static bool prefill_store(const bench_scenario *scenario, const key_slot *slots, const unsigned char *value, run_counters *counters)
{
    for (size_t index = 0; index < scenario->keyspace_size; ++index)
    {
        if (!is_set_success(set_one(&slots[index], value, scenario->value_size, counters)))
        {
            return false;
        }
    }
    return true;
}

static bool execute_operation(const bench_scenario *scenario,
                              const key_slot *slots,
                              const unsigned char *value,
                              prng_state *rng,
                              size_t op_index,
                              run_counters *counters)
{
    size_t key_index = sample_key_index(scenario, rng, op_index);
    if (scenario->kind == BENCH_SET_NEW)
    {
        size_t keyspace = scenario->keyspace_size == 0 ? 1 : scenario->keyspace_size;
        // Ensure SET_NEW keeps inserting unseen keys until keyspace is exhausted.
        key_index = op_index % keyspace;
    }
    const key_slot *slot = &slots[key_index];

    switch (scenario->kind)
    {
    case BENCH_SET_NEW:
    {
        return is_set_success(set_one(slot, value, scenario->value_size, counters));
    }
    case BENCH_SET_UPDATE:
    {
        return is_set_success(set_one(slot, value, scenario->value_size, counters));
    }
    case BENCH_GET_HIT:
    {
        return get_hit(slot, value, scenario->value_size, counters) == SUCCESS;
    }
    case BENCH_GET_MISS:
    {
        return get_miss(slot, counters) != SUCCESS;
    }
    case BENCH_DELETE:
    {
        return delete_one(slot, counters) == SUCCESS;
    }
    case BENCH_MIXED:
    {
        const size_t ratio_scale = 100;
        size_t set_limit = (size_t)(scenario->set_ratio * (double)ratio_scale);
        size_t get_limit = set_limit + (size_t)(scenario->get_ratio * (double)ratio_scale);
        size_t phase = op_index % ratio_scale;

        if (phase < set_limit)
        {
            return is_set_success(set_one(slot, value, scenario->value_size, counters));
        }
        if (phase < get_limit)
        {
            return get_hit(slot, value, scenario->value_size, counters) == SUCCESS;
        }
        return delete_one(slot, counters) == SUCCESS;
    }
    }

    return false;
}

static bool run_timed_phase(const bench_scenario *scenario,
                            const key_slot *slots,
                            const unsigned char *value,
                            prng_state *rng,
                            double duration_seconds,
                            bool collect_latency,
                            bool collect_timeline,
                            run_counters *counters,
                            size_t *operations_executed,
                            latency_store *latencies,
                            timeline_store *timeline,
                            uint64_t *elapsed_ns_out)
{
    struct timespec start_ts;
    struct timespec now_ts;
    struct timespec op_start;
    struct timespec op_end;
    uint64_t phase_ns = (uint64_t)(duration_seconds * 1000000000.0);
    uint64_t timeline_ns = 100000000ULL;

    if (clock_gettime(CLOCK_MONOTONIC, &start_ts) != 0)
    {
        return false;
    }

    uint64_t slice_start_ns = 0;
    unsigned int slice_resize_start = counters->resize_count;
    size_t op_index = 0;
    unsigned int max_chain_depth = 0;
    latency_store slice_latencies = {0};
    slice_latencies.max_samples = 4096;

    while (true)
    {
        if (clock_gettime(CLOCK_MONOTONIC, &now_ts) != 0)
        {
            break;
        }

        uint64_t elapsed_ns = ns_diff(&start_ts, &now_ts);
        if (elapsed_ns >= phase_ns)
        {
            if (collect_timeline)
            {
                if (slice_latencies.count > 0)
                {
                    qsort(slice_latencies.values, slice_latencies.count, sizeof(uint64_t), compare_uint64);
                }

                timeline_slice final_slice = {
                    .p99_ns = percentile(slice_latencies.values, slice_latencies.count, 0.99),
                    .resize_delta = counters->resize_count - slice_resize_start,
                    .max_chain_depth = max_chain_depth,
                    .operations = op_index};
                (void)timeline_store_append(timeline, final_slice);
            }
            *elapsed_ns_out = elapsed_ns;
            break;
        }

        if (collect_latency)
        {
            clock_gettime(CLOCK_MONOTONIC, &op_start);
        }

        (void)execute_operation(scenario, slots, value, rng, op_index, counters);

        if (collect_latency)
        {
            clock_gettime(CLOCK_MONOTONIC, &op_end);
            uint64_t op_ns = ns_diff(&op_start, &op_end);
            (void)latency_store_append(latencies, op_ns);
            if (collect_timeline)
            {
                (void)latency_store_append(&slice_latencies, op_ns);
            }
        }

        ++op_index;

        if (collect_timeline)
        {
            unsigned int current_depth = 0;
            if (current_depth > max_chain_depth)
            {
                max_chain_depth = current_depth;
            }

            if (elapsed_ns - slice_start_ns >= timeline_ns)
            {
                if (slice_latencies.count > 0)
                {
                    qsort(slice_latencies.values, slice_latencies.count, sizeof(uint64_t), compare_uint64);
                }

                timeline_slice slice = {
                    .p99_ns = percentile(slice_latencies.values, slice_latencies.count, 0.99),
                    .resize_delta = counters->resize_count - slice_resize_start,
                    .max_chain_depth = max_chain_depth,
                    .operations = op_index};
                (void)timeline_store_append(timeline, slice);

                slice_start_ns = elapsed_ns;
                slice_resize_start = counters->resize_count;
                max_chain_depth = 0;
                slice_latencies.count = 0;
            }
        }
    }

    free(slice_latencies.values);
    *operations_executed = op_index;
    return true;
}

static void summarize_latency(latency_store *store, bench_result *result)
{
    if (store->count == 0)
    {
        result->latency_p50_ns = 0;
        result->latency_p95_ns = 0;
        result->latency_p99_ns = 0;
        result->latency_max_ns = 0;
        return;
    }

    qsort(store->values, store->count, sizeof(uint64_t), compare_uint64);
    result->latency_p50_ns = percentile(store->values, store->count, 0.50);
    result->latency_p95_ns = percentile(store->values, store->count, 0.95);
    result->latency_p99_ns = percentile(store->values, store->count, 0.99);
    result->latency_max_ns = store->values[store->count - 1];
}

static char *build_resize_timeline(const timeline_store *timeline)
{
    if (timeline->count == 0)
    {
        return NULL;
    }

    size_t capacity = (timeline->count * 80) + 1;
    char *buffer = (char *)calloc(capacity, sizeof(char));
    if (buffer == NULL)
    {
        return NULL;
    }

    size_t offset = 0;
    for (size_t index = 0; index < timeline->count; ++index)
    {
        const timeline_slice *slice = &timeline->slices[index];
        int written = snprintf(buffer + offset,
                               capacity - offset,
                               "%s%zu:%zu:%" PRIu64 ":%u:%u",
                               index == 0 ? "" : "|",
                               index,
                               slice->operations,
                               slice->p99_ns,
                               slice->resize_delta,
                               slice->max_chain_depth);
        if (written < 0)
        {
            free(buffer);
            return NULL;
        }

        size_t advance = (size_t)written;
        if (offset + advance >= capacity)
        {
            free(buffer);
            return NULL;
        }

        offset += advance;
    }

    return buffer;
}

int run_single_thread_scenario(const bench_scenario *scenario, bench_result *result)
{
    memset(result, 0, sizeof(*result));
    result->scenario = scenario;
    result->latency_measured = scenario->measure_latency;

    if (scenario->keyspace_size == 0 || scenario->measure_seconds <= 0.0)
    {
        return ERR_INVALID_ARGUMENT;
    }

    if (initialise_key_store(scenario->config, scenario->pre_allocation_factor) != SUCCESS)
    {
        return ERR_FAILURE;
    }

    unsigned long rss_start_kb = read_current_rss_kb();
    key_slot *slots = create_key_slots(scenario->test_id, scenario->keyspace_size, scenario->low_entropy_keys);
    unsigned char *value_buffer = create_value_buffer(scenario->value_size, (unsigned char)(hash_string(scenario->test_id) & 0xFFU));
    latency_store latencies = {0};
    latencies.max_samples = 200000;
    timeline_store timeline = {0};
    run_counters counters = {0};
    bool ok = true;

    if (slots == NULL || value_buffer == NULL)
    {
        free_key_slots(slots, scenario->keyspace_size);
        free(value_buffer);
        cleanup_key_store();
        return ERR_MEMORY_ALLOCATION_FAILED;
    }

    if (scenario->variant == BENCH_VARIANT_WARM)
    {
        ok = prefill_store(scenario, slots, value_buffer, &counters);
    }

    prng_state rng = {.state = hash_string(scenario->test_id) ^ 0x9E3779B97F4A7C15ULL};

    if (ok)
    {
        size_t warmup_ops = 0;
        uint64_t warmup_elapsed_ns = 0;
        ok = run_timed_phase(scenario,
                             slots,
                             value_buffer,
                             &rng,
                             scenario->warmup_seconds,
                             false,
                             false,
                             &counters,
                             &warmup_ops,
                             &latencies,
                             &timeline,
                             &warmup_elapsed_ns);
    }

    size_t measured_ops = 0;
    uint64_t measured_elapsed_ns = 0;
    if (ok)
    {
        ok = run_timed_phase(scenario,
                             slots,
                             value_buffer,
                             &rng,
                             scenario->measure_seconds,
                             scenario->measure_latency,
                             scenario->collect_resize_timeline,
                             &counters,
                             &measured_ops,
                             &latencies,
                             &timeline,
                             &measured_elapsed_ns);
    }

    result->verification_passed = ok && counters.failure_count == 0 && counters.missing_count == 0;
    result->elapsed_seconds = (double)measured_elapsed_ns / 1000000000.0;
    result->measured_operations = measured_ops;
    result->throughput_ops_sec = result->elapsed_seconds > 0.0 ? ((double)measured_ops / result->elapsed_seconds) : 0.0;
    result->peak_rss_kb = read_peak_rss_kb();
    unsigned long rss_end_kb = read_current_rss_kb();
    result->rss_delta_kb = (rss_end_kb > rss_start_kb) ? (rss_end_kb - rss_start_kb) : 0UL;
    result->resize_count = counters.resize_count;
    result->success_count = counters.success_count;
    result->failure_count = counters.failure_count;
    result->missing_count = counters.missing_count;
    result->max_chain_depth = 0;

    summarize_latency(&latencies, result);
    result->resize_events_timeline = build_resize_timeline(&timeline);

    free(latencies.values);
    free(timeline.slices);
    free(value_buffer);
    free_key_slots(slots, scenario->keyspace_size);
    cleanup_key_store();
    return ok ? SUCCESS : ERR_FAILURE;
}

void print_single_thread_result_json(FILE *stream, const bench_result *result)
{
    const bench_scenario *scenario = result->scenario;
    const char *variant = scenario->variant == BENCH_VARIANT_COLD ? "cold" : "warm";
    const char *distribution = "uniform";

    if (scenario->distribution == BENCH_DIST_ZIPF)
    {
        distribution = "zipf";
    }
    else if (scenario->distribution == BENCH_DIST_BURSTY)
    {
        distribution = "bursty";
    }

    fprintf(stream,
            "{\"run_id\":\"%s\",\"run_index\":%u,\"test_id\":\"%s\",\"title\":\"%s\",\"category\":\"%s\",\"kind\":%d,\"bucket_size\":%u,\"sub_bucket_size\":%u,\"max_chain_length\":%u,\"pre_allocation_factor\":%.2f,\"warmup_seconds\":%.2f,\"measure_seconds\":%.2f,\"variant\":\"%s\",\"distribution\":\"%s\",\"value_size_bytes\":%zu,\"keyspace_size\":%zu,\"operations\":%zu,\"elapsed_seconds\":%.6f,\"mean_ops_sec\":%.2f,\"median_ops_sec\":%.2f,\"stdev_ops_sec\":%.2f,\"cv_ops_sec\":%.6f,\"throughput_ops_sec\":%.2f,\"peak_rss_kb\":%lu,\"rss_delta_kb\":%lu,\"resize_count\":%u,\"max_chain_depth\":%u,\"resize_events_timeline\":\"%s\",\"success_count\":%lu,\"failure_count\":%lu,\"missing_count\":%lu,\"verification_passed\":%s,\"latency_measured\":%s,\"latency_p50_ns\":%" PRIu64 ",\"latency_p95_ns\":%" PRIu64 ",\"latency_p99_ns\":%" PRIu64 ",\"latency_max_ns\":%" PRIu64 "}\n",
            result->run_id == NULL ? "" : result->run_id,
            result->run_index,
            scenario->test_id,
            scenario->title,
            scenario->category,
            (int)scenario->kind,
            scenario->config.bucket_size,
            scenario->config.sub_hash_table_bucket_size,
            scenario->config.max_linked_list_chain_length,
            scenario->pre_allocation_factor,
            scenario->warmup_seconds,
            scenario->measure_seconds,
            variant,
            distribution,
            scenario->value_size,
            scenario->keyspace_size,
            result->measured_operations,
            result->elapsed_seconds,
            result->mean_ops_sec,
            result->median_ops_sec,
            result->stdev_ops_sec,
            result->cv_ops_sec,
            result->throughput_ops_sec,
            result->peak_rss_kb,
            result->rss_delta_kb,
            result->resize_count,
            result->max_chain_depth,
            result->resize_events_timeline == NULL ? "" : result->resize_events_timeline,
            result->success_count,
            result->failure_count,
            result->missing_count,
            result->verification_passed ? "true" : "false",
            result->latency_measured ? "true" : "false",
            result->latency_p50_ns,
            result->latency_p95_ns,
            result->latency_p99_ns,
            result->latency_max_ns);
}