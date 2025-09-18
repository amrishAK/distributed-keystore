#include "core/key_store.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdatomic.h>

#define NUM_THREADS 20
#define NUM_KEYS_PER_THREAD 20

static atomic_int race_errors = 0;
static atomic_int undeleted_keys = 0;

// Thread context
typedef struct {
    int thread_id;
    char key_prefix[16];
} thread_ctx;

// Each thread sets and gets a unique set of keys (no overlap)
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
        int set_result = set_key(key, &kv);
        if(set_result != 0) {
            printf("[Thread %d] Failed to set key %s\n due to %d", ctx->thread_id, key, set_result);
            atomic_fetch_add(&race_errors, 1);
            continue;
        }
        key_store_value out = {0};
        int get_result = get_key(key, &out);
        if (get_result != 0) {
            printf("[Thread %d] Key missing after set (bucket-level concurrency) on key %s\n", ctx->thread_id, key);
            atomic_fetch_add(&race_errors, 1);
        }
        if (out.data) free((void *)out.data); // Use custom allocator if required by your API
    }
    return NULL;
}



int main() {
    printf("Starting concurrency stress test...\n");
    if(initialise_key_store(1024, 0.5, true) != 0) {
        printf("Failed to initialize key store with concurrency enabled.\n");
        return -1;
    }

    pthread_t threads[NUM_THREADS];
    thread_ctx ctxs[NUM_THREADS];

    // Launch set/get threads (no overlap, bucket-level concurrency)
    for (int i = 0; i < NUM_THREADS; ++i) {
        ctxs[i].thread_id = i;
        snprintf(ctxs[i].key_prefix, sizeof(ctxs[i].key_prefix), "T%d", i);
        pthread_create(&threads[i], NULL, thread_set_get, &ctxs[i]);
    }
    for (int i = 0; i < NUM_THREADS; ++i) {
        pthread_join(threads[i], NULL);
    }

    cleanup_key_store();
    printf("==== Concurrency Test Report ====\n");
    printf("Total threads: %d\n", NUM_THREADS);
    printf("Number of keys per thread: %d\n", NUM_KEYS_PER_THREAD);
    printf("Key missing after set (bucket-level concurrency): %d\n", race_errors);
    if (race_errors == 0) {
        printf("Result: PASS\n");
    } else {
        printf("Result: FAIL\n");
    }
    printf("=================================\n");
    printf("Bucket-level concurrency test completed.\n");
    return 0;
}
