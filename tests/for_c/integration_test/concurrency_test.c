#include "core/key_store.h"
#include "core/type_definition.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <stdatomic.h>
#include <time.h>
#include <inttypes.h>


#define NUM_THREADS 1000
#define NUM_KEYS_PER_THREAD 1000
#define MAX_OPS (NUM_THREADS * NUM_KEYS_PER_THREAD)

static uint64_t set_latencies_ns[MAX_OPS];
static uint64_t get_latencies_ns[MAX_OPS];
static atomic_int set_latency_idx = 0;
static atomic_int get_latency_idx = 0;

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
    key_store_value kv;
    kv.data = value;
    kv.data_size = sizeof(value);

    int start = ctx->thread_id * NUM_KEYS_PER_THREAD;
    int end = start + NUM_KEYS_PER_THREAD;
    for (int i = start; i < end; ++i) {
        snprintf(key, sizeof(key), "K%d", i);
        memset(value, ctx->thread_id, sizeof(value));
        kv.data_size = sizeof(value);

        struct timespec t1, t2;
        clock_gettime(CLOCK_MONOTONIC, &t1);
        int set_result = set_key(key, &kv);
        clock_gettime(CLOCK_MONOTONIC, &t2);
        if(set_result != 0) {
            printf("[Thread %d] Failed to set key %s\n due to %d", ctx->thread_id, key, set_result);
            atomic_fetch_add(&race_errors, 1);
        } else {
            int idx = atomic_fetch_add(&set_latency_idx, 1);
            if (idx < MAX_OPS) set_latencies_ns[idx] = timespec_diff_ns(&t1, &t2);
        }

        key_store_value out = {0};
        clock_gettime(CLOCK_MONOTONIC, &t1);
        int get_result = get_key(key, &out);
        clock_gettime(CLOCK_MONOTONIC, &t2);
        if (get_result != 0) {
            printf("[Thread %d] Key missing after set (bucket-level concurrency) on key %s\n", ctx->thread_id, key);
            atomic_fetch_add(&race_errors, 1);
        } else {
            int idx = atomic_fetch_add(&get_latency_idx, 1);
            if (idx < MAX_OPS) get_latencies_ns[idx] = timespec_diff_ns(&t1, &t2);
        }
        if (out.data) free((void *)out.data); // Use custom allocator if required by your API
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
    if(initialise_key_store(1024, 1, true) != 0) {
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

    // --- Statistics ---
    keystore_stats stats = get_keystore_stats();

    int set_count = atomic_load(&set_latency_idx);
    int get_count = atomic_load(&get_latency_idx);
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
    printf("\n-- Metadata --\n");
    printf("Init timestamp: %llu\n", (unsigned long long)stats.metadata.init_timestamp);
    printf("Last cleanup timestamp: %llu\n", (unsigned long long)stats.metadata.last_cleanup_timestamp);
    printf("\n-- Key/Entry Statistics --\n");
    printf("Total keys: %u\n", stats.key_entries.total_keys);
    printf("Total buckets: %u\n", stats.key_entries.total_buckets);
    printf("Non-empty buckets: %u\n", stats.key_entries.nonempty_buckets);
    printf("Empty buckets: %u\n", stats.key_entries.empty_buckets);
    printf("Max keys in a bucket: %u\n", stats.key_entries.max_keys_in_bucket);
    printf("Min keys in a bucket: %u\n", stats.key_entries.min_keys_in_bucket);
    printf("Average keys per non-empty bucket: %.2f\n", stats.key_entries.avg_keys_per_nonempty_bucket);
    printf("Stddev keys per bucket: %.2f\n", stats.key_entries.stddev_keys_per_bucket);
    printf("Median keys per bucket: %.2f\n", stats.key_entries.median_keys_per_bucket);
    printf("Average collisions per non-empty bucket: %.2f\n", stats.key_entries.avg_collisions_per_nonempty_bucket);
    printf("Empty bucket percent: %.2f%%\n", stats.key_entries.empty_bucket_percent);
    printf("\n-- Collision Statistics --\n");
    printf("Buckets with collisions (>1 key): %u\n", stats.collisions.collision_buckets);
    printf("Collision percent: %.2f%%\n", stats.collisions.collision_percent);
    printf("Highest collision in a bucket: %u\n", stats.collisions.highest_collision_in_bucket);
    printf("Average collisions per non-empty bucket: %.2f\n", stats.collisions.avg_collisions_per_nonempty_bucket);
    printf("\n-- Memory Pool Statistics --\n");
    printf("Total memory bytes: %zu\n", stats.memory_pool.total_memory_bytes);
    printf("Used memory bytes: %zu\n", stats.memory_pool.used_memory_bytes);
    printf("Free memory bytes: %zu\n", stats.memory_pool.free_memory_bytes);
    printf("Memory utilization percent: %.2f%%\n", stats.memory_pool.memory_utilization_percent);
    printf("Memory per key bytes: %zu\n", stats.memory_pool.memory_per_key_bytes);
    printf("Fragmentation percent: %.2f%%\n", stats.memory_pool.fragmentation_percent);
    printf("\n-- Bucket Operation Counters --\n");
    printf("Total add ops: %lu\n", stats.operation_counters.total_add_ops);
    printf("Total find ops: %lu\n", stats.operation_counters.total_find_ops);
    printf("Total delete ops: %lu\n", stats.operation_counters.total_delete_ops);
    printf("Failed add ops: %lu\n", stats.operation_counters.failed_add_ops);
    printf("Failed find ops: %lu\n", stats.operation_counters.failed_find_ops);
    printf("Failed delete ops: %lu\n", stats.operation_counters.failed_delete_ops);
    printf("Key missing after set (bucket-level concurrency): %d\n", race_errors);
    printf("Operation error code breakdown:\n");
    for(int i = 0; i < 100; ++i) {
        if(stats.operation_counters.error_code_counters[i] > 0) {
            printf("Error code -%d occurred %lu times\n", i, stats.operation_counters.error_code_counters[i]);
        }
    }

    printf("\n-- Data Node Operation Counters --\n");

    printf("Total update ops: %lu\n", stats.data_node_counters.total_update_ops);
    printf("Total read ops: %lu\n", stats.data_node_counters.total_read_ops);
    printf("Total delete ops: %lu\n", stats.data_node_counters.total_delete_ops);
    printf("Total create ops: %lu\n", stats.data_node_counters.total_create_ops);
    printf("Failed update ops: %lu\n", stats.data_node_counters.failed_update_ops);
    printf("Failed read ops: %lu\n", stats.data_node_counters.failed_read_ops);
    printf("Failed delete ops: %lu\n", stats.data_node_counters.failed_delete_ops);
    printf("Failed create ops: %lu\n", stats.data_node_counters.failed_create_ops);
    printf("Data node operation error code breakdown:\n"); 
    for(int i = 0; i < 100; ++i) {
        if(stats.data_node_counters.error_code_counters[i] > 0) {
            printf("Error code -%d occurred %lu times\n", i, stats.data_node_counters.error_code_counters[i]);
        }
    }
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
