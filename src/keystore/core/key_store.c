#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "key_store.h"
#include "hash/hash_functions.h"
#include "utils/memory_manager.h"
#include "utils/helper_functions.h"
#include "hash_table/hash_table_operation.h"

#include <time.h>


#pragma region Private Global Variables
static uint64_t g_bucket_hash_seed = 0; // Global seed for bucket hash function, initialized during key store initialization
static uint64_t g_sub_bucket_hash_seed = 0; // Global seed for sub-bucket hash function, initialized during key store initialization
hash_table_memory_pool* g_hash_table_pool = NULL;
#pragma endregion


#pragma region Private Function Declarations
static int _get_key_hash(const char *key, composite_key_hash *key_hash_out);
#pragma endregion

#pragma region Public Function Definitions
int initialise_key_store(hash_table_configuration config, double pre_memory_allocation_factor) 
{ 
    if (g_hash_table_pool != NULL) return SUCCESS; // Already initialized — idempotent re-entry

    if(pre_memory_allocation_factor < 0.0 || pre_memory_allocation_factor > 1.0) return ERR_INVALID_ARGUMENT; // Error handling: invalid pre-allocation factor

    if (config.bucket_size == 0 || config.sub_hash_table_bucket_size == 0  || config.max_linked_list_chain_length == 0)  return ERR_INVALID_CONFIG; // Error handling: invalid configuration

    if(!is_power_of_two(config.bucket_size) || !is_power_of_two(config.sub_hash_table_bucket_size)) return ERR_INVALID_CONFIG; // Error handling: bucket size must be a power of two

    memory_manager_config memory_manager_config = {
        .bucket_size = config.bucket_size,
        .sub_bucket_size = config.sub_hash_table_bucket_size,
        .pre_allocation_factor = pre_memory_allocation_factor,
        .allocate_list_pool = true,
        .is_concurrency_enabled = config.is_concurrency_enabled
    };

    int memory_manager_init_result = initialize_memory_manager(memory_manager_config);
    if (memory_manager_init_result != 0) {
        return memory_manager_init_result; // Error handling: failed to initialize memory manager
    }

    // Generate random seeds for hash functions to ensure different hash distributions across runs.
    // XOR the sub-bucket seed with a golden-ratio constant to guarantee the two seeds are always
    // distinct, even when both calls resolve to the same nanosecond timestamp.
    g_bucket_hash_seed     = generate_hash_seed();
    g_sub_bucket_hash_seed = derive_distinct_seed(generate_hash_seed());

    int hash_buckets_init_result = create_new_hash_table(config, &g_hash_table_pool);
    if( hash_buckets_init_result != 0) {
        cleanup_memory_manager();
        return hash_buckets_init_result; // Error handling: failed to create hash table
    }

    return SUCCESS; // Success
}


int cleanup_key_store(void) 
{
    // Cleanup hash table and free associated memory
    cleanup_hash_table(g_hash_table_pool);
    free_memory(g_hash_table_pool, false);
    g_hash_table_pool = NULL;

    // Cleanup memory manager
    cleanup_memory_manager();

    // Reset global hash seeds
    g_bucket_hash_seed = 0;
    g_sub_bucket_hash_seed = 0;

    return SUCCESS; // Success
}


int set_key(key_value_pair* value) 
{
    if (value == NULL || value->value == NULL || value->value_size == 0 || value->key == NULL || value->key[0] == '\0') return ERR_INVALID_ARGUMENT; // Error handling: invalid input

    composite_key_hash key_hash;

    int get_key_hash_result = _get_key_hash(value->key, &key_hash);
    if (get_key_hash_result != 0) return get_key_hash_result; // Error handling: failed to get hash and index
    
    return upsert_node_to_hash_table(g_hash_table_pool, key_hash, value);
}


int get_key(const char *key, key_value_pair *value_out) 
{
    if (key == NULL || key[0] == '\0' || value_out == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input

    composite_key_hash key_hash;
    int get_hash_result = _get_key_hash(key, &key_hash);
    if (get_hash_result != 0) return get_hash_result; // Error handling: failed to get hash and index

    return get_key_value_from_hash_table(g_hash_table_pool, key_hash, key, value_out);
}


int delete_key(const char *key) 
{
    if (key == NULL || key[0] == '\0') return ERR_INVALID_ARGUMENT; // Error handling: invalid input

    composite_key_hash key_hash;
    int get_hash_result = _get_key_hash(key, &key_hash);
    if (get_hash_result != 0) return get_hash_result; // Error handling: failed to get hash and index

    return delete_key_from_hash_table(g_hash_table_pool, key_hash, key);
}

#pragma endregion

#pragma region Private Function Definitions

/**
 * @fn _get_key_hash
 * @brief Generates a composite hash for the given key.
 *
 * This function generates a composite hash based on the key, using separate seeds
 * for the bucket and sub-bucket hashes. The composite hash is used to efficiently
 * locate the key within the hash table.
 *
 * @param key The key for which to generate the hash.
 * @param key_hash_out Pointer to a composite_key_hash structure to receive the generated hash.
 * @return 0 on success, or a negative error code on failure.
 */
int _get_key_hash(const char *key, composite_key_hash *key_hash_out) 
{
    if ( key_hash_out == NULL) return ERR_INVALID_ARGUMENT; // Handle error: invalid output pointers

    composite_key_hash key_hash = {0};
    key_hash.bucket_hash = hash_function_murmur_64(key, g_bucket_hash_seed);
    key_hash.sub_bucket_hash = hash_function_murmur_64(key, g_sub_bucket_hash_seed);


    if( key_hash.bucket_hash == UINT64_MAX || key_hash.sub_bucket_hash == UINT64_MAX) return ERR_HASH_COMPUTE_FAILED; // Handle error: hash function failed
    
    *key_hash_out = key_hash;
    return SUCCESS; // Success
}

#pragma endregion
