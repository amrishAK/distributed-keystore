#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "key_store.h"
#include "hash/hash_functions.h"
#include "utils/memory_manager.h"
#include "utils/helper_functions.h"
#include "hash_table/hash_table_operation.h"

#pragma region Private Global Variables
static uint32_t g_hash_seed = 0;
hash_table_memory_pool* g_hash_table_pool = NULL;
#pragma endregion


#pragma region Private Function Declarations
static uint32_t _generate_hash_seed(void);
static int _get_hash_and_index(const char *key, uint32_t *key_hash_out);
#pragma endregion

#pragma region Public Function Definitions
int initialise_key_store(hash_table_configuration config, double pre_memory_allocation_factor) 
{ 
    if (config.bucket_size == 0 || config.sub_hash_table_block_size == 0  || config.max_linked_list_Chain_length == 0 || pre_memory_allocation_factor < 0.0 || pre_memory_allocation_factor > 1.0) {
        return -10; // Error handling: invalid configuration
    }

    if(!is_power_of_two(config.bucket_size) || !is_power_of_two(config.sub_hash_table_block_size)) {
        return -11; // Error handling: bucket size must be a power of two
    }

    memory_manager_config memory_manager_config = {
        .bucket_size = config.bucket_size,
        .sub_bucket_size = config.sub_hash_table_block_size,
        .pre_allocation_factor = pre_memory_allocation_factor,
        .allocate_list_pool = true,
        .is_concurrency_enabled = config.is_concurrency_enabled
    };

    int memory_manager_init_result = initialize_memory_manager(memory_manager_config);
    if (memory_manager_init_result != 0) {
        return memory_manager_init_result; // Error handling: failed to initialize memory manager
    }

    g_hash_seed = _generate_hash_seed();

    int hash_buckets_init_result = create_new_hash_table(config, &g_hash_table_pool);
    if( hash_buckets_init_result != 0) {
        cleanup_memory_manager();
        return hash_buckets_init_result; // Error handling: failed to create hash table
    }

    return 0;
}


int cleanup_key_store(void) 
{
    cleanup_hash_buckets();
    cleanup_memory_manager();
    g_hash_seed = 0;
    return 0;
}


int set_key(key_value_pair* value) 
{
    if (value == NULL || value->value == NULL || value->value_size == 0 || value->key == NULL || value->key[0] == '\0') return -20; // Error handling: invalid input

    uint32_t key_hash;

    int get_hash_result = _get_hash_and_index(value->key, &key_hash);
    if (get_hash_result != 0) return get_hash_result; // Error handling: failed to get hash and index
    
    return upsert_node_to_hash_table(g_hash_table_pool, key_hash, value);
}


int get_key(const char *key, key_value_pair *value_out) 
{
    if (key == NULL || key[0] == '\0' || value_out == NULL) return -20; // Error handling: invalid input

    uint32_t key_hash;
    int get_hash_result = _get_hash_and_index(key, &key_hash);
    if (get_hash_result != 0) return get_hash_result; // Error handling: failed to get hash and index

    return get_key_value_from_hash_table(g_hash_table_pool, key_hash, key, value_out);
}


int delete_key(const char *key) 
{
    if (key == NULL || key[0] == '\0') return -20; // Error handling: invalid input

    uint32_t key_hash;
    int get_hash_result = _get_hash_and_index(key, &key_hash);
    if (get_hash_result != 0) return get_hash_result; // Error handling: failed to get hash and index

    return delete_key_from_hash_table(g_hash_table_pool, key_hash, key);
}

#pragma endregion

#pragma region Private Function Definitions

/**
 * @fn _generate_hash_seed
 * @brief Generates a random seed for the hash function.
 *
 * This function generates a random seed based on the current time.
 * The seed is used to initialize the hash function to ensure different
 * hash distributions across different runs of the program.
 *
 * @return A 32-bit unsigned integer representing the generated hash seed.
 */
uint32_t _generate_hash_seed(void) 
{
    // Simple seed generation using current time
    return (uint32_t)time(NULL);
}


/**
 * @fn _get_hash_and_index
 * @brief Generates a random seed for the hash function.
 *
 * This function generates a random seed based on the current time.
 * The seed is used to initialize the hash function to ensure different
 * hash distributions across different runs of the program.
 *
 * @return A 32-bit unsigned integer representing the generated hash seed.
 */
int _get_hash_and_index(const char *key, uint32_t *key_hash_out) 
{
    if ( key_hash_out == NULL) return -20; // Handle error: invalid output pointers

    uint32_t key_hash = hash_function_murmur_32(key, g_hash_seed);

    if(key_hash == UINT32_MAX) return -70; // Handle error: hash function failed
    
    *key_hash_out = key_hash;
    return 0;
}

#pragma endregion
