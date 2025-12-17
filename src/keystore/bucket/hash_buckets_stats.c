#include "core/type_definition.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>

#pragma region Private Function Declarations
static double _calculate_standard_deviation_keys_per_bucket(key_entry_stats* stats, hash_bucket_memory_pool* pool_ptr);
static double _calculate_median_keys_per_bucket(key_entry_stats* stats, hash_bucket_memory_pool* pool_ptr);
#pragma endregion

#pragma region Private Function Definitions


/**
 * @fn _calculate_standard_deviation_keys_per_bucket
 * @brief Calculates the standard deviation of keys per non-empty bucket.
 *
 * This function computes the standard deviation of the number of keys
 * in each non-empty hash bucket based on the average keys per non-empty bucket.
 *
 * @param stats Pointer to key_entry_stats structure containing average keys per non-empty bucket.
 * @note Assumes that stats->avg_keys_per_nonempty_bucket is already populated.
 * @return double The standard deviation of keys per non-empty bucket.
 */
static double _calculate_standard_deviation_keys_per_bucket(key_entry_stats* stats, hash_bucket_memory_pool* pool_ptr) {
    if (stats->nonempty_buckets == 0) return 0.0;

    double sum_squared_diff = 0.0;
    for (unsigned int i = 0; i < pool_ptr->total_blocks; ++i) {
        hash_bucket *bucket_ptr = &pool_ptr->hash_buckets_ptr[i];
        if (bucket_ptr->is_initialized && bucket_ptr->count > 0) {
            double diff = bucket_ptr->count - stats->avg_keys_per_nonempty_bucket;
            sum_squared_diff += diff * diff;
        }
    }

    return sqrt(sum_squared_diff / stats->nonempty_buckets);
}


/**
 * @fn _calculate_median_keys_per_bucket
 * @brief Calculates the median number of keys per non-empty bucket.
 *
 * This function collects the key counts from all non-empty buckets,
 * sorts them, and computes the median value.
 *
 * @param stats Pointer to key_entry_stats structure containing non-empty bucket count.
 * @note Assumes that stats->nonempty_buckets is already populated.
 * @return double The median number of keys per non-empty bucket.
 */
static double _calculate_median_keys_per_bucket(key_entry_stats* stats, hash_bucket_memory_pool* pool_ptr) {

    if (stats->nonempty_buckets == 0) return 0.0;

    unsigned int *keys_per_bucket = malloc(stats->nonempty_buckets * sizeof(unsigned int));
    if (keys_per_bucket == NULL) return 0.0; // Memory allocation failure

    unsigned int idx = 0;
    for (unsigned int i = 0; i < pool_ptr->total_blocks; ++i) {
        hash_bucket *bucket_ptr = &pool_ptr->hash_buckets_ptr[i];
        if (bucket_ptr->is_initialized && bucket_ptr->count > 0) {
            keys_per_bucket[idx++] = bucket_ptr->count;
        }
    }

    // Sort the array to find median
    qsort(keys_per_bucket, stats->nonempty_buckets, sizeof(unsigned int), (int (*)(const void*, const void*))strcmp);

    double median;
    if (stats->nonempty_buckets % 2 == 0) {
        median = (keys_per_bucket[stats->nonempty_buckets / 2 - 1] + keys_per_bucket[stats->nonempty_buckets / 2]) / 2.0;
    } else {
        median = keys_per_bucket[stats->nonempty_buckets / 2];
    }

    free(keys_per_bucket);
    return median;
}

#pragma endregion

#pragma region Public Function Definitions

/**
 * @fn _calculate_key_entry_stats
 * @brief Calculates statistics about key entries in the hash bucket memory pool.
 *
 * This function iterates through all hash buckets in the memory pool,
 * collecting statistics such as total keys, non-empty buckets, max/min keys
 * in a bucket, average keys per non-empty bucket, standard deviation, median,
 * and empty bucket percentage.
 *
 * @return key_entry_stats A struct containing the calculated statistics.
 */
key_entry_stats _calculate_key_entry_stats(hash_bucket_memory_pool* pool_ptr) {
    key_entry_stats entry_stats = {0};

    unsigned int total_keys = 0;
    unsigned int nonempty_buckets = 0;
    unsigned int max_keys_in_bucket = 0;
    unsigned int min_keys_in_bucket = UINT32_MAX;

    for (unsigned int i = 0; i < pool_ptr->total_blocks; ++i) {
        hash_bucket *bucket_ptr = &pool_ptr->hash_buckets_ptr[i];
        if (bucket_ptr->is_initialized) {
            entry_stats.total_buckets++;
            if (bucket_ptr->count > 0) {
                nonempty_buckets++;
                total_keys += bucket_ptr->count;
                if (bucket_ptr->count > max_keys_in_bucket) {
                    max_keys_in_bucket = bucket_ptr->count;
                }
                if (bucket_ptr->count < min_keys_in_bucket) {
                    min_keys_in_bucket = bucket_ptr->count;
                }
            }
        }
    }

    entry_stats.total_keys = total_keys;
    entry_stats.nonempty_buckets = nonempty_buckets;
    entry_stats.empty_buckets = entry_stats.total_buckets - nonempty_buckets;
    entry_stats.max_keys_in_bucket = max_keys_in_bucket;
    entry_stats.min_keys_in_bucket = (min_keys_in_bucket == UINT32_MAX) ? 0 : min_keys_in_bucket;
    entry_stats.avg_keys_per_nonempty_bucket = (nonempty_buckets > 0) ? ((double)total_keys / nonempty_buckets) : 0.0;
    entry_stats.empty_bucket_percent = (entry_stats.total_buckets > 0) ? ((double)entry_stats.empty_buckets / entry_stats.total_buckets) * 100.0 : 0.0;
    entry_stats.avg_collisions_per_nonempty_bucket = (nonempty_buckets > 0) ? ((double)(total_keys - nonempty_buckets) / nonempty_buckets) : 0.0;
    entry_stats.median_keys_per_bucket = _calculate_median_keys_per_bucket(&entry_stats, pool_ptr);
    entry_stats.stddev_keys_per_bucket = _calculate_standard_deviation_keys_per_bucket(&entry_stats, pool_ptr);

    return entry_stats;
}


/**
 * @fn _calculate_collision_stats
 * @brief Calculates statistics about key collisions in the hash bucket memory pool.
 *
 * This function iterates through all hash buckets in the memory pool,
 * collecting statistics such as number of collision buckets, highest
 * collision in a bucket, average collisions per non-empty bucket, and
 * collision percentage.
 *
 * @return key_collision_stats A struct containing the calculated statistics.
 */
key_collision_stats _calculate_collision_stats(hash_bucket_memory_pool* pool_ptr) {
    key_collision_stats collision_stats = {0};
    unsigned int collision_buckets = 0;
    unsigned int highest_collision_in_bucket = 0;
    double sum_collisions = 0.0;

    for (unsigned int i = 0; i < pool_ptr->total_blocks; ++i) {
        hash_bucket *bucket_ptr = &pool_ptr->hash_buckets_ptr[i];
        if (bucket_ptr->is_initialized && bucket_ptr->count > 1) {
            unsigned int collisions = bucket_ptr->count - 1;
            collision_buckets++;
            sum_collisions += collisions;
            if (collisions > highest_collision_in_bucket) {
                highest_collision_in_bucket = collisions;
            }
        }
    }

    collision_stats.collision_buckets = collision_buckets;
    collision_stats.collision_percent = (pool_ptr->total_blocks > 0) ? ((double)collision_buckets / pool_ptr->total_blocks) * 100.0 : 0.0;
    collision_stats.highest_collision_in_bucket = highest_collision_in_bucket;
    collision_stats.avg_collisions_per_nonempty_bucket = (collision_buckets > 0) ? (sum_collisions / collision_buckets) : 0.0;

    return collision_stats;
}

/**
 * @fn _calculate_memory_stats
 * @brief Calculates memory usage statistics for the hash bucket memory pool.
 *
 * This function computes total memory, used memory, free memory,
 * memory utilization percentage, and memory per key based on the
 * current state of the hash bucket memory pool.
 *
 * @param total_keys The total number of keys stored in the hash buckets.
 * @return memory_pool_stats A struct containing the calculated memory statistics.
 */
memory_pool_stats _calculate_memory_stats(hash_bucket_memory_pool* pool_ptr, unsigned int total_keys) {
    memory_pool_stats mem_stats = {0};
    size_t total_memory_bytes = pool_ptr->total_blocks * pool_ptr->block_size;
    size_t used_memory_bytes = 0;

    for (unsigned int i = 0; i < pool_ptr->total_blocks; ++i) {
        hash_bucket *bucket_ptr = &pool_ptr->hash_buckets_ptr[i];
        if (bucket_ptr->is_initialized) {
            used_memory_bytes += sizeof(hash_bucket);
            // Additional memory used by nodes can be added here if tracked
        }
    }

    size_t free_memory_bytes = total_memory_bytes - used_memory_bytes;
    double memory_utilization_percent = (total_memory_bytes > 0) ? ((double)used_memory_bytes / total_memory_bytes) * 100.0 : 0.0;
    size_t memory_per_key_bytes = (used_memory_bytes > 0 && total_keys > 0) ? (used_memory_bytes / total_keys) : 0;
    
    mem_stats.total_memory_bytes = total_memory_bytes;
    mem_stats.used_memory_bytes = used_memory_bytes;
    mem_stats.free_memory_bytes = free_memory_bytes;
    mem_stats.memory_utilization_percent = memory_utilization_percent;
    mem_stats.memory_per_key_bytes = memory_per_key_bytes;

    return mem_stats;
}

#pragma endregion