#define _POSIX_C_SOURCE 200809L

#include "multi_thread_runner.h"

#include <inttypes.h>
#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
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
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    unsigned int participants;
    unsigned int arrived;
    unsigned int generation;
} sync_barrier;

typedef struct
{
    uint64_t *values;
    size_t capacity;
    atomic_size_t count;
} shared_latency_store;

typedef struct
{
    uint64_t *latencies;
    size_t latency_count;
    size_t latency_capacity;
    unsigned long operations;
    unsigned long resize_delta;
    unsigned int max_chain_depth;
} timeline_slice;

typedef struct
{
    timeline_slice *slices;
    size_t count;
    pthread_mutex_t mutex;
} timeline_store;

typedef struct
{
    uint64_t state;
} prng_state;

typedef struct
{
    const mt_scenario *scenario;
    unsigned int thread_index;
    uint64_t start_ns;
    uint64_t warmup_end_ns;
    uint64_t measure_end_ns;
    sync_barrier *barrier;
    shared_latency_store *latencies;
    timeline_store *timeline;
} thread_context;

static atomic_ulong g_set_count = 0;
static atomic_ulong g_get_count = 0;
static atomic_ulong g_delete_count = 0;
static atomic_ulong g_failure_count = 0;
static atomic_ulong g_missing_count = 0;
static atomic_ulong g_resize_count = 0;

static bool
is_set_success(int result)
{
    return result == SUCCESS ||
           result == SUCESS_ADDED_NEW_NODE ||
           result == SUCCESS_ADDED_TO_PENDING_LIST ||
           result == SUCESS_ADDED_NEW_NODE_RESZING_TRIGGERED;
}

static uint64_t
hash_string(const char *text)
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

static uint64_t
prng_next(prng_state *rng)
{
    uint64_t x = rng->state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    rng->state = x;
    return x * 2685821657736338717ULL;
}

static double
prng_uniform(prng_state *rng)
{
    return (double)(prng_next(rng) >> 11) * (1.0 / 9007199254740992.0);
}

static uint64_t
clock_now_ns(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint64_t)now.tv_sec * 1000000000ULL + (uint64_t)now.tv_nsec;
}

static void
wait_until_ns(uint64_t deadline_ns)
{
    for (;;)
    {
        uint64_t now_ns = clock_now_ns();
        if (now_ns >= deadline_ns)
        {
            return;
        }

        uint64_t remaining_ns = deadline_ns - now_ns;
        struct timespec delay = {
            .tv_sec = (time_t)(remaining_ns / 1000000000ULL),
            .tv_nsec = (long)(remaining_ns % 1000000000ULL)};

        if (delay.tv_sec == 0 && delay.tv_nsec < 1000000L)
        {
            continue;
        }

        nanosleep(&delay, NULL);
    }
}

static int
barrier_init(sync_barrier *barrier, unsigned int participants)
{
    if (pthread_mutex_init(&barrier->mutex, NULL) != 0)
    {
        return ERR_RESOURCE_INIT_FAILED;
    }
    if (pthread_cond_init(&barrier->condition, NULL) != 0)
    {
        pthread_mutex_destroy(&barrier->mutex);
        return ERR_RESOURCE_INIT_FAILED;
    }

    barrier->participants = participants;
    barrier->arrived = 0;
    barrier->generation = 0;
    return SUCCESS;
}

static void
barrier_destroy(sync_barrier *barrier)
{
    pthread_cond_destroy(&barrier->condition);
    pthread_mutex_destroy(&barrier->mutex);
}

static void
barrier_wait(sync_barrier *barrier)
{
    pthread_mutex_lock(&barrier->mutex);
    unsigned int generation = barrier->generation;
    ++barrier->arrived;

    if (barrier->arrived == barrier->participants)
    {
        barrier->arrived = 0;
        ++barrier->generation;
        pthread_cond_broadcast(&barrier->condition);
    }
    else
    {
        while (generation == barrier->generation)
        {
            pthread_cond_wait(&barrier->condition, &barrier->mutex);
        }
    }

    pthread_mutex_unlock(&barrier->mutex);
}

static int
shared_latency_store_init(shared_latency_store *store, size_t capacity)
{
    memset(store, 0, sizeof(*store));
    store->values = (uint64_t *)calloc(capacity, sizeof(uint64_t));
    if (store->values == NULL)
    {
        return ERR_MEMORY_ALLOCATION_FAILED;
    }

    store->capacity = capacity;
    atomic_init(&store->count, 0);
    return SUCCESS;
}

static void
shared_latency_store_record(shared_latency_store *store, uint64_t latency_ns)
{
    size_t slot = atomic_fetch_add(&store->count, 1);
    if (slot < store->capacity)
    {
        store->values[slot] = latency_ns;
    }
}

static int
timeline_store_init(timeline_store *timeline, double measure_seconds)
{
    memset(timeline, 0, sizeof(*timeline));
    if (pthread_mutex_init(&timeline->mutex, NULL) != 0)
    {
        return ERR_RESOURCE_INIT_FAILED;
    }

    size_t slices = (size_t)ceil(measure_seconds * 10.0);
    if (slices == 0)
    {
        slices = 1;
    }

    timeline->slices = (timeline_slice *)calloc(slices, sizeof(timeline_slice));
    if (timeline->slices == NULL)
    {
        pthread_mutex_destroy(&timeline->mutex);
        return ERR_MEMORY_ALLOCATION_FAILED;
    }

    timeline->count = slices;
    for (size_t index = 0; index < slices; ++index)
    {
        timeline->slices[index].latency_capacity = 4096;
        timeline->slices[index].latencies = (uint64_t *)calloc(timeline->slices[index].latency_capacity, sizeof(uint64_t));
        if (timeline->slices[index].latencies == NULL)
        {
            for (size_t cleanup_index = 0; cleanup_index < index; ++cleanup_index)
            {
                free(timeline->slices[cleanup_index].latencies);
            }
            free(timeline->slices);
            pthread_mutex_destroy(&timeline->mutex);
            return ERR_MEMORY_ALLOCATION_FAILED;
        }
    }

    return SUCCESS;
}

static void
timeline_store_destroy(timeline_store *timeline)
{
    if (timeline->slices != NULL)
    {
        for (size_t index = 0; index < timeline->count; ++index)
        {
            free(timeline->slices[index].latencies);
        }
    }
    free(timeline->slices);
    pthread_mutex_destroy(&timeline->mutex);
}

static void
timeline_store_record(timeline_store *timeline, uint64_t elapsed_measure_ns, unsigned long operations, unsigned long resize_delta, uint64_t latency_ns)
{
    if (timeline == NULL || timeline->count == 0)
    {
        return;
    }

    size_t index = (size_t)(elapsed_measure_ns / 100000000ULL);
    if (index >= timeline->count)
    {
        index = timeline->count - 1;
    }

    pthread_mutex_lock(&timeline->mutex);
    timeline_slice *slice = &timeline->slices[index];
    slice->operations += operations;
    slice->resize_delta += resize_delta;
    if (slice->latency_count < slice->latency_capacity)
    {
        slice->latencies[slice->latency_count++] = latency_ns;
    }
    pthread_mutex_unlock(&timeline->mutex);
}

static char *
build_resize_timeline(const timeline_store *timeline)
{
    if (timeline == NULL || timeline->count == 0)
    {
        return NULL;
    }

    size_t capacity = (timeline->count * 64U) + 1U;
    char *buffer = (char *)calloc(capacity, sizeof(char));
    if (buffer == NULL)
    {
        return NULL;
    }

    size_t offset = 0;
    for (size_t index = 0; index < timeline->count; ++index)
    {
        timeline_slice slice = timeline->slices[index];
        uint64_t p99_ns = 0;
        if (slice.latency_count > 0)
        {
            qsort(slice.latencies, slice.latency_count, sizeof(uint64_t), compare_uint64);
            p99_ns = percentile(slice.latencies, slice.latency_count, 0.99);
        }

        int written = snprintf(buffer + offset,
                               capacity - offset,
                               "%s%zu:%lu:%" PRIu64 ":%lu:%u",
                               index == 0 ? "" : "|",
                               index,
                               slice.operations,
                               p99_ns,
                               slice.resize_delta,
                               slice.max_chain_depth);
        if (written < 0)
        {
            free(buffer);
            return NULL;
        }

        if ((size_t)written >= capacity - offset)
        {
            free(buffer);
            return NULL;
        }

        offset += (size_t)written;
    }

    return buffer;
}

static size_t
normalized_key_index(const mt_scenario *scenario, size_t key_index)
{
    size_t keyspace = scenario->keyspace_size == 0 ? 1 : scenario->keyspace_size;
    if (scenario->low_entropy_keys)
    {
        size_t entropy_window = keyspace < 256 ? keyspace : 256;
        if (entropy_window == 0)
        {
            entropy_window = 1;
        }
        return key_index % entropy_window;
    }
    return key_index % keyspace;
}

static void
build_key(char *buffer, size_t size, const mt_scenario *scenario, size_t key_index)
{
    snprintf(buffer, size, "K%08zu", normalized_key_index(scenario, key_index));
}

static void
fill_value(unsigned char *buffer, size_t size, size_t key_index)
{
    unsigned char seed = (unsigned char)(key_index * 17U + 31U);
    for (size_t index = 0; index < size; ++index)
    {
        buffer[index] = (unsigned char)(seed + (unsigned char)(index & 0x3FU));
    }
}

static size_t
sample_key_index(const mt_scenario *scenario, prng_state *rng, size_t op_index)
{
    size_t keyspace = scenario->keyspace_size == 0 ? 1 : scenario->keyspace_size;

    if (scenario->distribution == MT_DIST_ZIPF)
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

        double rank = pow(u, 1.0 / (1.0 - theta));
        size_t index = (size_t)(rank * (double)keyspace);
        if (index >= keyspace)
        {
            index = keyspace - 1;
        }
        return index;
    }

    if (scenario->distribution == MT_DIST_BURSTY)
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
    }

    return (size_t)(prng_next(rng) % keyspace);
}

static void
record_failure(bool missing)
{
    atomic_fetch_add(&g_failure_count, 1UL);
    if (missing)
    {
        atomic_fetch_add(&g_missing_count, 1UL);
    }
}

static bool
do_set(const mt_scenario *scenario, const char *key, const unsigned char *value, unsigned long *resize_delta, bool counted)
{
    key_value_pair pair = {
        .key = (char *)key,
        .value = (unsigned char *)value,
        .value_size = scenario->value_size};

    int result = set_key(&pair);
    if (!is_set_success(result))
    {
        record_failure(true);
        return false;
    }

    if (counted)
    {
        atomic_fetch_add(&g_set_count, 1UL);
    }

    if (result == SUCESS_ADDED_NEW_NODE_RESZING_TRIGGERED)
    {
        atomic_fetch_add(&g_resize_count, 1UL);
        *resize_delta += 1UL;
    }

    return true;
}

static bool
do_get(const mt_scenario *scenario, const char *key, const unsigned char *expected_value, bool counted)
{
    key_value_pair out = {0};
    int result = get_key(key, &out);
    bool ok = result == SUCCESS && out.value != NULL && out.value_size == scenario->value_size &&
              memcmp(out.value, expected_value, scenario->value_size) == 0;
    if (!ok)
    {
        record_failure(true);
    }
    else if (counted)
    {
        atomic_fetch_add(&g_get_count, 1UL);
    }

    free_kv_pair(&out);
    return ok;
}

static bool
do_delete(const char *key, bool counted)
{
    int result = delete_key(key);
    if (result != SUCCESS)
    {
        record_failure(true);
        return false;
    }

    if (counted)
    {
        atomic_fetch_add(&g_delete_count, 1UL);
    }
    return true;
}

static bool
prefill_store(const mt_scenario *scenario)
{
    char key_buffer[64];
    unsigned char *value_buffer = (unsigned char *)malloc(scenario->value_size);
    if (value_buffer == NULL)
    {
        return false;
    }

    for (size_t key_index = 0; key_index < scenario->keyspace_size; ++key_index)
    {
        build_key(key_buffer, sizeof(key_buffer), scenario, key_index);
        fill_value(value_buffer, scenario->value_size, key_index);
        unsigned long resize_delta = 0;
        if (!do_set(scenario, key_buffer, value_buffer, &resize_delta, false))
        {
            free(value_buffer);
            return false;
        }
    }

    free(value_buffer);
    return true;
}

static void
summarize_latency(shared_latency_store *latencies, mt_result *result)
{
    size_t count = atomic_load(&latencies->count);
    if (count > latencies->capacity)
    {
        count = latencies->capacity;
    }

    if (count == 0)
    {
        result->latency_p50_ns = 0;
        result->latency_p95_ns = 0;
        result->latency_p99_ns = 0;
        result->latency_max_ns = 0;
        return;
    }

    qsort(latencies->values, count, sizeof(uint64_t), compare_uint64);
    result->latency_p50_ns = percentile(latencies->values, count, 0.50);
    result->latency_p95_ns = percentile(latencies->values, count, 0.95);
    result->latency_p99_ns = percentile(latencies->values, count, 0.99);
    result->latency_max_ns = latencies->values[count - 1];
}

static void *
run_thread(void *arg)
{
    thread_context *context = (thread_context *)arg;
    const mt_scenario *scenario = context->scenario;
    unsigned char *value_buffer = (unsigned char *)malloc(scenario->value_size);
    unsigned char *expected_buffer = (unsigned char *)malloc(scenario->value_size);
    char key_buffer[64];
    size_t op_index = 0;
    prng_state rng = {.state = hash_string(scenario->test_id) ^ ((uint64_t)context->thread_index + 1ULL) * 0x9E3779B97F4A7C15ULL};

    if (value_buffer == NULL || expected_buffer == NULL)
    {
        record_failure(true);
        free(value_buffer);
        free(expected_buffer);
        return NULL;
    }

    barrier_wait(context->barrier);
    wait_until_ns(context->start_ns);

    while (clock_now_ns() < context->measure_end_ns)
    {
        uint64_t op_begin_ns = clock_now_ns();
        if (op_begin_ns >= context->measure_end_ns)
        {
            break;
        }

        bool measured = op_begin_ns >= context->warmup_end_ns;
        size_t logical_op_index = op_index++;
        size_t key_index = sample_key_index(scenario, &rng, logical_op_index);
        if (scenario->kind == MT_RESIZE_WRITE)
        {
            size_t keyspace = scenario->keyspace_size == 0 ? 1 : scenario->keyspace_size;
            // Give each thread a disjoint key stream to maximize new inserts before wraparound.
            key_index = ((logical_op_index * (size_t)scenario->threads) + (size_t)context->thread_index) % keyspace;
        }
        build_key(key_buffer, sizeof(key_buffer), scenario, key_index);
        fill_value(value_buffer, scenario->value_size, key_index);
        fill_value(expected_buffer, scenario->value_size, key_index);

        unsigned long resize_delta = 0;
        unsigned long op_weight = 0;

        switch (scenario->kind)
        {
            case MT_SCALE_PAIR:
                do_set(scenario, key_buffer, value_buffer, &resize_delta, measured);
                do_get(scenario, key_buffer, expected_buffer, measured);
                op_weight = measured ? 2UL : 0UL;
                break;
            case MT_MIXED_READ_WRITE:
            {
                const size_t ratio_scale = 100U;
                size_t set_limit = (size_t)(scenario->set_ratio * (double)ratio_scale);
                size_t get_limit = set_limit + (size_t)(scenario->get_ratio * (double)ratio_scale);
                size_t phase = logical_op_index % ratio_scale;

                if (phase < set_limit)
                {
                    do_set(scenario, key_buffer, value_buffer, &resize_delta, measured);
                }
                else if (phase < get_limit)
                {
                    do_get(scenario, key_buffer, expected_buffer, measured);
                }
                else
                {
                    do_delete(key_buffer, measured);
                }
                op_weight = measured ? 1UL : 0UL;
                break;
            }
            case MT_RESIZE_WRITE:
                do_set(scenario, key_buffer, value_buffer, &resize_delta, measured);
                op_weight = measured ? 1UL : 0UL;
                break;
            case MT_SOFT_DELETE:
                do_delete(key_buffer, measured);
                op_weight = measured ? 1UL : 0UL;
                break;
        }

        if (measured && scenario->measure_latency)
        {
            uint64_t op_end_ns = clock_now_ns();
            uint64_t latency_ns = op_end_ns - op_begin_ns;
            shared_latency_store_record(context->latencies, latency_ns);
            if (scenario->collect_resize_timeline && context->timeline != NULL)
            {
                timeline_store_record(context->timeline, op_end_ns - context->warmup_end_ns, op_weight, resize_delta, latency_ns);
            }
        }
    }

    free(value_buffer);
    free(expected_buffer);
    return NULL;
}

int
run_multi_thread_scenario(const mt_scenario *scenario, mt_result *result)
{
    memset(result, 0, sizeof(*result));
    result->scenario = scenario;

    if (initialise_key_store(scenario->config, scenario->pre_allocation_factor) != SUCCESS)
    {
        return ERR_FAILURE;
    }

    atomic_store(&g_set_count, 0UL);
    atomic_store(&g_get_count, 0UL);
    atomic_store(&g_delete_count, 0UL);
    atomic_store(&g_failure_count, 0UL);
    atomic_store(&g_missing_count, 0UL);
    atomic_store(&g_resize_count, 0UL);

    unsigned long rss_start_kb = read_current_rss_kb();
    if (scenario->variant == MT_VARIANT_WARM && !prefill_store(scenario))
    {
        cleanup_key_store();
        return ERR_FAILURE;
    }

    shared_latency_store latencies = {0};
    timeline_store timeline = {0};
    bool timeline_initialized = false;
    if (shared_latency_store_init(&latencies, 200000U) != SUCCESS)
    {
        cleanup_key_store();
        return ERR_MEMORY_ALLOCATION_FAILED;
    }

    if (scenario->collect_resize_timeline)
    {
        if (timeline_store_init(&timeline, scenario->measure_seconds) != SUCCESS)
        {
            free(latencies.values);
            cleanup_key_store();
            return ERR_MEMORY_ALLOCATION_FAILED;
        }
        timeline_initialized = true;
    }

    pthread_t *threads = (pthread_t *)calloc(scenario->threads, sizeof(pthread_t));
    thread_context *contexts = (thread_context *)calloc(scenario->threads, sizeof(thread_context));
    sync_barrier barrier;
    if (threads == NULL || contexts == NULL || barrier_init(&barrier, scenario->threads) != SUCCESS)
    {
        free(threads);
        free(contexts);
        if (timeline_initialized)
        {
            timeline_store_destroy(&timeline);
        }
        free(latencies.values);
        cleanup_key_store();
        return ERR_MEMORY_ALLOCATION_FAILED;
    }

    uint64_t now_ns = clock_now_ns();
    uint64_t start_ns = now_ns + 50000000ULL;
    uint64_t warmup_end_ns = start_ns + (uint64_t)(scenario->warmup_seconds * 1000000000.0);
    uint64_t measure_end_ns = warmup_end_ns + (uint64_t)(scenario->measure_seconds * 1000000000.0);

    for (unsigned int thread_index = 0; thread_index < scenario->threads; ++thread_index)
    {
        contexts[thread_index] = (thread_context){
            .scenario = scenario,
            .thread_index = thread_index,
            .start_ns = start_ns,
            .warmup_end_ns = warmup_end_ns,
            .measure_end_ns = measure_end_ns,
            .barrier = &barrier,
            .latencies = &latencies,
            .timeline = timeline_initialized ? &timeline : NULL};

        if (pthread_create(&threads[thread_index], NULL, run_thread, &contexts[thread_index]) != 0)
        {
            for (unsigned int joined = 0; joined < thread_index; ++joined)
            {
                pthread_join(threads[joined], NULL);
            }
            barrier_destroy(&barrier);
            free(threads);
            free(contexts);
            if (timeline_initialized)
            {
                timeline_store_destroy(&timeline);
            }
            free(latencies.values);
            cleanup_key_store();
            return ERR_THREAD_CREATION_FAILED;
        }
    }

    for (unsigned int thread_index = 0; thread_index < scenario->threads; ++thread_index)
    {
        pthread_join(threads[thread_index], NULL);
    }

    barrier_destroy(&barrier);

    result->timed_operations = atomic_load(&g_set_count) + atomic_load(&g_get_count) + atomic_load(&g_delete_count);
    result->set_operations = atomic_load(&g_set_count);
    result->get_operations = atomic_load(&g_get_count);
    result->delete_operations = atomic_load(&g_delete_count);
    result->failure_count = atomic_load(&g_failure_count);
    result->missing_count = atomic_load(&g_missing_count);
    result->resize_count = atomic_load(&g_resize_count);
    result->elapsed_seconds = scenario->measure_seconds;
    result->throughput_ops_sec = result->elapsed_seconds > 0.0 ? ((double)result->timed_operations / result->elapsed_seconds) : 0.0;
    result->peak_rss_kb = read_peak_rss_kb();
    {
        unsigned long rss_end_kb = read_current_rss_kb();
        result->rss_delta_kb = rss_end_kb > rss_start_kb ? (rss_end_kb - rss_start_kb) : 0UL;
    }
    result->verification_passed = result->failure_count == 0UL && result->missing_count == 0UL;
    result->latency_measured = scenario->measure_latency;
    result->max_chain_depth = get_key_store_max_chain_depth();
    summarize_latency(&latencies, result);
    if (timeline_initialized)
    {
        for (size_t index = 0; index < timeline.count; ++index)
        {
            timeline.slices[index].max_chain_depth = result->max_chain_depth;
        }
        result->resize_events_timeline = build_resize_timeline(&timeline);
    }

    free(threads);
    free(contexts);
    if (timeline_initialized)
    {
        timeline_store_destroy(&timeline);
    }
    free(latencies.values);
    cleanup_key_store();
    return result->verification_passed ? SUCCESS : ERR_FAILURE;
}

void
print_multi_thread_result_json(FILE *stream, const mt_result *result)
{
    const mt_scenario *scenario = result->scenario;
    const char *variant = scenario->variant == MT_VARIANT_COLD ? "cold" : "warm";
    const char *distribution = "uniform";

    if (scenario->distribution == MT_DIST_ZIPF)
    {
        distribution = "zipf";
    }
    else if (scenario->distribution == MT_DIST_BURSTY)
    {
        distribution = "bursty";
    }

        fprintf(stream,
            "{\"run_id\":\"%s\",\"run_index\":%u,\"test_id\":\"%s\",\"family\":\"%s\",\"category\":\"%s\",\"title\":\"%s\",\"workload\":\"%s\",\"kind\":%d,\"repetitions\":%u,\"warmup_duration_seconds\":%.2f,\"measurement_duration_seconds\":%.2f,\"threads\":%u,\"variant\":\"%s\",\"key_distribution\":\"%s\",\"distribution\":\"%s\",\"bucket_size\":%u,\"sub_bucket_size\":%u,\"max_chain_length\":%u,\"pre_allocation_factor\":%.2f,\"keyspace_size\":%zu,\"value_size_bytes\":%zu,\"set_ratio\":%.2f,\"get_ratio\":%.2f,\"delete_ratio\":%.2f,\"timed_operations\":%lu,\"elapsed_seconds\":%.6f,\"mean_ops_sec\":%.2f,\"median_ops_sec\":%.2f,\"stdev_ops_sec\":%.2f,\"cv_ops_sec\":%.6f,\"throughput_ops_sec\":%.2f,\"peak_rss_kb\":%lu,\"rss_delta_kb\":%lu,\"resize_count\":%lu,\"max_chain_depth\":%u,\"resize_events_timeline\":\"%s\",\"set_operations\":%lu,\"get_operations\":%lu,\"delete_operations\":%lu,\"failure_count\":%lu,\"missing_count\":%lu,\"verification_passed\":%s,\"latency_measured\":%s,\"latency_p50_ns\":%" PRIu64 ",\"latency_p95_ns\":%" PRIu64 ",\"latency_p99_ns\":%" PRIu64 ",\"latency_max_ns\":%" PRIu64 "}\n",
            result->run_id == NULL ? "" : result->run_id,
            result->run_index,
            scenario->test_id,
            scenario->family,
            scenario->family,
            scenario->title,
            scenario->workload,
            (int)scenario->kind,
            scenario->repetitions,
            scenario->warmup_seconds,
            scenario->measure_seconds,
            scenario->threads,
            variant,
            distribution,
            distribution,
            scenario->config.bucket_size,
            scenario->config.sub_hash_table_bucket_size,
            scenario->config.max_linked_list_chain_length,
            scenario->pre_allocation_factor,
            scenario->keyspace_size,
            scenario->value_size,
            scenario->set_ratio,
            scenario->get_ratio,
            scenario->delete_ratio,
            result->timed_operations,
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
            result->set_operations,
            result->get_operations,
            result->delete_operations,
            result->failure_count,
            result->missing_count,
            result->verification_passed ? "true" : "false",
            result->latency_measured ? "true" : "false",
            result->latency_p50_ns,
            result->latency_p95_ns,
            result->latency_p99_ns,
            result->latency_max_ns);
}