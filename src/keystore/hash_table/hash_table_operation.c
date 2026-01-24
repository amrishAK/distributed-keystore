#include "hash_table_operation.h"
#include "hash_bucket_operation.h"
#include "type_definitions/error_code_definitions.h"
#include "utils/helper_functions.h"
#include "utils/memory_manager.h"
#include <stdlib.h>
#include <stdio.h>

#pragma region Private Function Declarations
static int _get_hash_table_bucket(hash_table_memory_pool* hash_table_ptr, uint32_t key_hash, hash_bucket** hash_bucket_out);
#pragma endregion

#pragma region Public Functions Definitions

int create_new_hash_table(hash_table_configuration config, hash_table_memory_pool** hash_table_out) {
    
    if(config.bucket_size == 0 || !is_power_of_two(config.bucket_size)) {
        return ERR_INVALID_CONFIG; // Error handling: bucket_size must be a power of two and greater than 0
    }

    if (*hash_table_out != NULL) {
        return ERR_INVALID_ARGUMENT; // Error handling: output pointer must be NULL
    }

    hash_table_memory_pool* new_memory_pool_ptr = allocate_memory(sizeof(hash_table_memory_pool));
    if (new_memory_pool_ptr == NULL) return ERR_MEMORY_ALLOCATION_FAILED; // Error handling: memory allocation failed

    new_memory_pool_ptr->hash_buckets_ptr = callocate_memory(config.bucket_size, sizeof(hash_bucket));
    if (new_memory_pool_ptr->hash_buckets_ptr == NULL) {
        free_memory(new_memory_pool_ptr, false);
        return ERR_MEMORY_ALLOCATION_FAILED; // Error handling: memory allocation failed  
    }

    sub_hash_table_configuration sub_hash_table_config = {
        .bucket_size = config.bucket_size,
        .is_concurrency_enabled = config.is_concurrency_enabled,
        .max_linked_list_chain_length = config.max_linked_list_chain_length
    };

    if(config.is_concurrency_enabled) {
        // Eager initialization of hash buckets if concurrency is enabled
        int init_result = 0;
        for (unsigned int i = 0; i < config.bucket_size; ++i) {
            init_result = initialise_hash_bucket(&new_memory_pool_ptr->hash_buckets_ptr[i], sub_hash_table_config);
            if (init_result != 0) {
                cleanup_hash_table(new_memory_pool_ptr);
                return init_result; // Error handling: failed to initialize hash bucket
            }
        }
    }

    new_memory_pool_ptr->block_size = sizeof(hash_bucket);
    new_memory_pool_ptr->is_initialized = true;
    new_memory_pool_ptr->total_blocks = config.bucket_size;
    new_memory_pool_ptr->sub_hash_table_config = sub_hash_table_config;

    *hash_table_out = new_memory_pool_ptr;
    return 0;
}

int cleanup_hash_table(hash_table_memory_pool* hash_table_ptr) {
    if (hash_table_ptr == NULL || !hash_table_ptr->is_initialized) return 0; // Nothing to clean up

    for (unsigned int i = 0; i < hash_table_ptr->total_blocks; ++i) {
        cleanup_hash_bucket(&hash_table_ptr->hash_buckets_ptr[i]);
    }

    free_memory(hash_table_ptr->hash_buckets_ptr, false);
    hash_table_ptr->hash_buckets_ptr = NULL;
    hash_table_ptr->is_initialized = false;
    hash_table_ptr->total_blocks = 0;
    hash_table_ptr->sub_hash_table_config = (sub_hash_table_configuration){0};
    return 0;
}

int upsert_node_to_hash_table(hash_table_memory_pool* hash_table_ptr, uint32_t key_hash, key_value_pair* kv_pair) {
    if(kv_pair == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input

    hash_bucket* target_hash_bucket;
    int bucket_result = _get_hash_table_bucket(hash_table_ptr, key_hash, &target_hash_bucket);
    if (bucket_result != 0) return bucket_result;

    return upsert_node_to_hash_bucket(target_hash_bucket, key_hash, kv_pair);
}

int get_key_value_from_hash_table(hash_table_memory_pool* hash_table_ptr, uint32_t key_hash, const char* key, key_value_pair* kv_pair_out) {
    if(kv_pair_out == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input

    hash_bucket* target_hash_bucket;
    int bucket_result = _get_hash_table_bucket(hash_table_ptr, key_hash, &target_hash_bucket);
    if (bucket_result != 0) return bucket_result;

    return get_key_value_from_hash_bucket(target_hash_bucket, key, key_hash, kv_pair_out);
}

int delete_key_from_hash_table(hash_table_memory_pool* hash_table_ptr, uint32_t key_hash, const char* key) {
    if(key == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input

    hash_bucket* target_hash_bucket;
    int bucket_result = _get_hash_table_bucket(hash_table_ptr, key_hash, &target_hash_bucket);
    if (bucket_result != 0) return bucket_result;

    return delete_key_from_hash_bucket(target_hash_bucket, key, key_hash);
}

#pragma endregion

#pragma region Private Function Definitions

/**
 * @fn _get_hash_table_bucket
 * @brief Retrieves the hash bucket from the hash table memory pool based on the key hash.
 * @param hash_table_ptr Pointer to the hash table memory pool.
 * @param key_hash Hash of the key.
 * @param hash_bucket_out Pointer to receive the found hash bucket.
 * @return int Returns 0 on success, or a negative value on failure.
 */
int _get_hash_table_bucket(hash_table_memory_pool* hash_table_ptr, uint32_t key_hash, hash_bucket** hash_bucket_out) {
    if(hash_table_ptr == NULL || key_hash == 0) return ERR_INVALID_ARGUMENT; // Error handling: invalid input

    if (!hash_table_ptr->is_initialized) return ERR_HASH_TABLE_NOT_INITIALIZED; // Error handling: hash-table not initialized

    unsigned int bucket_index = key_hash % hash_table_ptr->total_blocks;

    if(bucket_index >= hash_table_ptr->total_blocks) {
        return ERR_INVALID_ARGUMENT; // Error handling: invalid bucket index
    }

    hash_bucket* hash_bucket_ptr = &hash_table_ptr->hash_buckets_ptr[bucket_index];
    
    if(!hash_bucket_ptr->is_initialized) {
        int init_result = initialise_hash_bucket(hash_bucket_ptr, hash_table_ptr->sub_hash_table_config);
        if (init_result != 0) {
            cleanup_hash_bucket(hash_bucket_ptr);
            return init_result; // Error handling: failed to initialize hash bucket
        }
    }

    *hash_bucket_out = hash_bucket_ptr;
    return 0;
}

#pragma endregion