#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "key_store.h"
#include "data_node.h"
#include "bucket/hash_buckets.h"
#include "bucket/hash_bucket_list.h"
#include "hash/hash_functions.h"
#include "utils/memory_manager.h"

// global variables
static uint32_t g_hash_seed = 0;
static unsigned int g_bucket_size = 0;

// function declarations
static uint32_t _generate_hash_seed(void);
static int _get_bucket_index(uint32_t key_hash);
static int _get_hash_and_index(const char *key, uint32_t *key_hash_out, unsigned int *index_out);


int initialise_key_store(unsigned int bucket_size, double pre_memory_allocation_factor, bool is_concurrency_enabled) 
{ 
    if(bucket_size == 0 || pre_memory_allocation_factor < 0 || pre_memory_allocation_factor > 1) {
        return -1; // Invalid parameters
    }

    if(initialise_hash_buckets(bucket_size, is_concurrency_enabled) != 0) {
        return -1; // Failed to initialize hash buckets
    }

    memory_manager_config config = {
        .bucket_size = bucket_size,
        .pre_allocation_factor = pre_memory_allocation_factor,
        .allocate_list_pool = true,
        .allocate_tree_pool = false
    };

    if(initialize_memory_manager(config) != 0) {
        cleanup_hash_buckets();
        return -1; // Failed to initialize memory manager
    }

    g_hash_seed = _generate_hash_seed();
    g_bucket_size = bucket_size;
    return 0;
}


int cleanup_key_store(void) 
{
    cleanup_hash_buckets();
    cleanup_memory_manager();
    g_hash_seed = 0;
    return 0;
}


int set_key(const char *key, key_store_value value) 
{
    if (value.data == NULL || value.data_size == 0 || key == NULL || key[0] == '\0') {
        return -1; // Handle error: invalid input
    }

    uint32_t key_hash;
    unsigned int index;
    if (_get_hash_and_index(key, &key_hash, &index) != 0) {
        return -1; // Handle error: failed to get hash and index
    }

    data_node *existing_node = find_node_in_bucket(index, key, key_hash);

    if(existing_node != NULL) {
        return update_data_node(existing_node, value.data, value.data_size);
    }

    // Key does not exist, create a new node
    data_node *new_node = create_data_node(key, key_hash, value.data, value.data_size);
    if (new_node == NULL) {
        return -1; // Handle error: memory allocation failure
    }

    return add_node_to_bucket(index, key_hash, new_node);
}


int get_key(const char *key, key_store_value *value_out) 
{
    if (key == NULL || key[0] == '\0') {
        return -1; // Handle error: invalid input
    }

    uint32_t key_hash;
    unsigned int index;
    if (_get_hash_and_index(key, &key_hash, &index) != 0) {
        return -1; // Handle error: failed to get hash and index
    }

    data_node *node = find_node_in_bucket(index, key, key_hash);
    
    if (node == NULL) {
        value_out->data = NULL;
        value_out->data_size = 0;
        return -1; // Key not found
    }
    else {
        value_out->data = (unsigned char *)malloc(node->data_size);
        if (value_out->data == NULL) {
            return -1; // Memory allocation failure
        }
        memcpy((unsigned char *)value_out->data, node->data, node->data_size);
        value_out->data_size = node->data_size;
        return 0; // Success
    } 
}


int delete_key(const char *key) 
{    
    if (key == NULL || key[0] == '\0') {
        return -1; // Handle error: invalid input
    }

    uint32_t key_hash;
    unsigned int index;
    if (_get_hash_and_index(key, &key_hash, &index) != 0) {
        return -1; // Handle error: failed to get hash and index
    }

    return delete_node_from_bucket(index, key, key_hash);
}

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
 * @fn get_bucket_index
 * @brief Computes the bucket index for a given key hash.
 *
 * This function uses bitwise AND to map the key_hash to a valid bucket index
 * within the range [0, g_bucket_size - 1]. If the computed index is out of bounds,
 * it returns -1 to indicate an invalid index.
 *
 * @param key_hash The hash value of the key.
 * @return The bucket index corresponding to the key_hash, or -1 if invalid.
 */
int _get_bucket_index(uint32_t key_hash) 
{
    int index = key_hash & (g_bucket_size - 1);

    if(index < 0 || index >= g_bucket_size) {
        index = -1;
    }
    
    return index;
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
int _get_hash_and_index(const char *key, uint32_t *key_hash_out, unsigned int *index_out) 
{
    if ( key_hash_out == NULL || index_out == NULL) {
        return -1; // Handle error: invalid input
    }

    uint32_t key_hash = hash_function_murmur_32(key, g_hash_seed);
    int index = _get_bucket_index(key_hash);

    if(index < 0) {
        return -1; // Handle error: invalid bucket index
    }

    *key_hash_out = key_hash;
    *index_out = (unsigned int)index;
    return 0;
}
