#include "core/key_store.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <stdatomic.h>
#include <time.h>
#include <inttypes.h>

#include "type_definitions/error_code_definitions.h"
#include "type_definitions/sucess_code_definitions.h"
#include "utils/memory_manager.h"

#define NUM_THREADS 120
#define NUM_KEYS_PER_THREAD 150
#define MAX_OPS (NUM_THREADS * NUM_KEYS_PER_THREAD)

static uint64_t set_latencies_ns[MAX_OPS];
static uint64_t get_latencies_ns[MAX_OPS];
static atomic_int set_latency_idx = 0;
static atomic_int get_latency_idx = 0;
static atomic_int resizing_triggered_count = 0;

static atomic_int race_errors = 0;

// Thread context
typedef struct {
    int thread_id;
    char key_prefix[16];
} thread_ctx;

// Each thread sets and gets a unique set of keys (no overlap)

static inline uint64_t timespec_diff_ns(const struct timespec *start, const struct timespec *end) {
    return (uint64_t)(end->tv_sec - start->tv_sec) * 1000000000ULL + (end->tv_nsec - start->tv_nsec);
}

void *thread_set_get(void *arg) {
    thread_ctx *ctx = (thread_ctx *)arg;

    char key[32];
    unsigned char value[32];
    key_value_pair kv;
    kv.key = key;
    kv.value = value;
    kv.value_size = sizeof(value);

    int start = ctx->thread_id * NUM_KEYS_PER_THREAD;
    int end = start + NUM_KEYS_PER_THREAD;
    for (int i = start; i < end; ++i) {
        snprintf(key, sizeof(key), "K%d", i);
        memset(value, ctx->thread_id, sizeof(value));
        kv.value_size = sizeof(value);

        struct timespec t1, t2;
        clock_gettime(CLOCK_MONOTONIC, &t1);
        int set_result = set_key(&kv);
        clock_gettime(CLOCK_MONOTONIC, &t2);

        if(set_result == SUCESS_ADDED_NEW_NODE_RESZING_TRIGGERED) {
            atomic_fetch_add(&resizing_triggered_count, 1);
        }

        if(set_result == SUCCESS || set_result == SUCESS_ADDED_NEW_NODE || set_result == SUCCESS_ADDED_TO_PENDING_LIST || set_result == SUCESS_ADDED_NEW_NODE_RESZING_TRIGGERED) {
            int idx = atomic_fetch_add(&set_latency_idx, 1);
            if (idx < MAX_OPS) set_latencies_ns[idx] = timespec_diff_ns(&t1, &t2);
        } else {
            atomic_fetch_add(&race_errors, 1);
        }

        key_value_pair *out = callocate_memory(1, sizeof(key_value_pair));
        clock_gettime(CLOCK_MONOTONIC, &t1);
        int get_result = get_key(key, out);
        clock_gettime(CLOCK_MONOTONIC, &t2);
        if (get_result != 0) {
            atomic_fetch_add(&race_errors, 1);
        } else {
            int idx = atomic_fetch_add(&get_latency_idx, 1);
            if (idx < MAX_OPS) get_latencies_ns[idx] = timespec_diff_ns(&t1, &t2);
        }
        if (out->value) free((void *)out->value); // Use custom allocator if required by your API
        if (out->key) free((void *)out->key);
        free(out);
    }
    return NULL;
}
// Helper to compare uint64_t for qsort
static int cmp_uint64(const void *a, const void *b) {
    uint64_t va = *(const uint64_t *)a, vb = *(const uint64_t *)b;
    return (va > vb) - (va < vb);
}

void print_latency_report(const char *op, uint64_t *latencies, int count) {
    if (count == 0) {
        printf("No %s operations recorded.\n", op);
        return;
    }
    // Copy and sort
    uint64_t *tmp = malloc(count * sizeof(uint64_t));
    memcpy(tmp, latencies, count * sizeof(uint64_t));
    qsort(tmp, count, sizeof(uint64_t), cmp_uint64);
    double avg = 0.0;
    for (int i = 0; i < count; ++i) avg += tmp[i];
    avg /= count;
    uint64_t p50 = tmp[(int)(0.50 * count)];
    uint64_t p95 = tmp[(int)(0.95 * count)];
    uint64_t p99 = tmp[(int)(0.99 * count)];
    printf("%s latency (ns): avg=%.0f, p50=%" PRIu64 ", p95=%" PRIu64 ", p99=%" PRIu64 "\n", op, avg, p50, p95, p99);
    free(tmp);
}



int main() {
    
    printf("Starting concurrency stress test...\n");
    hash_table_configuration config = {
        .bucket_size = 1024,
        .is_concurrency_enabled = true,
        .sub_hash_table_bucket_size = 1024, // or another reasonable default
        .max_linked_list_chain_length = 20  // or another reasonable default
    };
    if(initialise_key_store(config, 1.0) != 0) {
        printf("Failed to initialize key store with concurrency enabled.\n");
        return -1;
    }


    pthread_t threads[NUM_THREADS];
    thread_ctx ctxs[NUM_THREADS];

    struct timespec global_start, global_end;
    clock_gettime(CLOCK_MONOTONIC, &global_start);

    // Launch set/get threads (no overlap, bucket-level concurrency)
    for (int i = 0; i < NUM_THREADS; ++i) {
        ctxs[i].thread_id = i;
        snprintf(ctxs[i].key_prefix, sizeof(ctxs[i].key_prefix), "T%d", i);
        pthread_create(&threads[i], NULL, thread_set_get, &ctxs[i]);
    }
    for (int i = 0; i < NUM_THREADS; ++i) {
        pthread_join(threads[i], NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &global_end);

    int set_count = atomic_load(&set_latency_idx);
    int get_count = atomic_load(&get_latency_idx);
    int resize_count = atomic_load(&resizing_triggered_count);
    uint64_t total_ns = timespec_diff_ns(&global_start, &global_end);
    double total_sec = total_ns / 1e9;
    double throughput = (set_count + get_count) / total_sec;

    printf("Test scenario: Bucket-level concurrency with %d threads each setting/getting %d unique keys.\n", NUM_THREADS, NUM_KEYS_PER_THREAD);
    printf("==== Concurrency Test Report ====\n");
    printf("Total threads: %d\n", NUM_THREADS);
    printf("Number of keys per thread: %d\n", NUM_KEYS_PER_THREAD);
    printf("Total ops: %d\n", set_count + get_count);
    printf("Total time: %.3fs\n", total_sec);
    printf("Throughput: %.2f ops/sec\n", throughput);
    print_latency_report("SET", set_latencies_ns, set_count);
    print_latency_report("GET", get_latencies_ns, get_count);
    extern atomic_int resizing_triggered_count;
    printf("Key missing after set (bucket-level concurrency): %d\n", race_errors);
    printf("Number of resizings triggered: %d\n", resize_count);
    printf("=================================\n");
    if (race_errors == 0) {
        printf("Result: PASS\n");
    } else {
        printf("Result: FAIL\n");
    }
    printf("=================================\n");
    printf("Bucket-level concurrency test completed.\n");

    
    cleanup_key_store();
    return 0;
}
