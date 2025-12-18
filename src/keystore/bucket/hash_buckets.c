/**
 * @file hash_buckets.c
 * @brief Implementation of hash bucket management for the key-value store.
 * This file provides functions to initialize, manage, and clean up hash buckets.
 * 
 * @note The hash bucket expects bucket size to be a power of two.
 * @note Concurrency control is optional and can be enabled or disabled during initialization.
 * @note This implementation currently supports only linked list based buckets.
 * @note This module encapsulates all operations related to hash buckets, including adding, finding, and deleting nodes.
 *
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "hash_buckets.h"
#include "hash_bucket_list.h"
#include "core/type_definition.h"
#include "core/data_node.h"
#include "utils/memory_manager.h"
#include "hash_buckets_operation.c"
#include "hash_buckets_stats.c"

#pragma region Private Global Variables
static hash_bucket_memory_pool g_hash_bucket_pool = {0};
#pragma endregion

#pragma region Private Function declarations
static bool _is_power_of_two(unsigned int n);
static int _initialise_hash_bucket(hash_bucket *hash_bucket_ptr);
static void _delete_hash_bucket(unsigned int index);
int _add_node_to_bucket(unsigned int index,  const char *key, uint32_t key_hash, key_store_value* new_value);
#pragma endregion

#pragma region Public Function Definitions

int initialise_hash_buckets(unsigned int bucket_size, bool is_concurrency_enabled) 
{    
    if (!_is_power_of_two(bucket_size))  return -21; // Error handling: bucket_size must be a power of two

    if (g_hash_bucket_pool.is_initialized) return 0; // Already initialized

    g_hash_bucket_pool.block_size = sizeof(hash_bucket);
    g_hash_bucket_pool.is_initialized = false;
    g_hash_bucket_pool.total_blocks = bucket_size;

    g_hash_bucket_pool.hash_buckets_ptr = calloc(bucket_size, sizeof(hash_bucket));

    if (g_hash_bucket_pool.hash_buckets_ptr == NULL)  return -10; // Error handling: memory allocation failed

    g_hash_bucket_pool.is_initialized = true;
    g_hash_bucket_pool.is_concurrency_enabled = is_concurrency_enabled;

    // Eager initialization of hash buckets if concurrency is enabled or else lazy initialization will be done
    int init_result = 0;
    if (is_concurrency_enabled) {
        for (unsigned int i = 0; i < bucket_size; ++i) {
            init_result = _initialise_hash_bucket(&g_hash_bucket_pool.hash_buckets_ptr[i]);
            if (init_result != 0) {
                cleanup_hash_buckets();
                return init_result; // Error handling: failed to initialize hash bucket
            }
        }
    }

    return 0;
}

int cleanup_hash_buckets(void) 
{
    if (!g_hash_bucket_pool.is_initialized) return 0; // Nothing to clean up

    // Clean up each hash bucket
    for(unsigned int i = 0; i < g_hash_bucket_pool.total_blocks; i++) 
    {
        _delete_hash_bucket(i);
    }

    // Free the memory pool
    free(g_hash_bucket_pool.hash_buckets_ptr);
    g_hash_bucket_pool.hash_buckets_ptr = NULL;
    g_hash_bucket_pool.block_size = 0;
    g_hash_bucket_pool.total_blocks = 0;
    g_hash_bucket_pool.is_initialized = false;
    g_hash_bucket_pool = (hash_bucket_memory_pool){0};
    
    return 0;
}


hash_bucket*  get_hash_bucket(unsigned int index) 
{
    if(!g_hash_bucket_pool.is_initialized || index >= g_hash_bucket_pool.total_blocks) return NULL; // Error handling: out of bounds or not initialized

    hash_bucket* target_bucket_ptr =  &g_hash_bucket_pool.hash_buckets_ptr[index];

    if(!target_bucket_ptr->is_initialized)
    {
        if (_initialise_hash_bucket(target_bucket_ptr) != 0) {
            _delete_hash_bucket(index); // or pass the pointer
            return NULL;
        }
    }

    return target_bucket_ptr;
}

int upsert_node_to_bucket(unsigned int index, const char *key, uint32_t key_hash, key_store_value* new_value)
{
    if (key == NULL || new_value == NULL) return -20; // Error handling: invalid input

    hash_bucket *hash_bucket_ptr = get_hash_bucket(index);
    if (hash_bucket_ptr == NULL) return -40; // Error handling: bucket not found or initialized

    data_node* data_node_ptr;
    bucket_operation_args input_args = {hash_bucket_ptr, key, key_hash, NULL};

    int result = 0;
    result = g_hash_bucket_pool.is_concurrency_enabled ? _hash_bucket_lock_wrapper(FIND_NODE, input_args, &data_node_ptr) : _find_node(input_args, &data_node_ptr);

    if(result == 0){
        // Node exists, update it
        result = g_hash_bucket_pool.is_concurrency_enabled ? data_node_mutex_lock_wrapper(DATA_NODE_UPDATE, data_node_ptr, new_value) : update_data_node(data_node_ptr, new_value);
    }
    else if (result == -41)
    {
        // Node does not exist, add it
        return _add_node_to_bucket(index, key, key_hash, new_value);
    }

    return result;
}


int find_node_in_bucket(unsigned int index, const char *key, uint32_t key_hash, key_store_value* value_out)
{
    if (key == NULL || value_out == NULL) return -20; // Error handling: invalid input

    hash_bucket *hash_bucket_ptr = get_hash_bucket(index);
    if (hash_bucket_ptr == NULL) return -40; // Error handling: bucket not found or initialized

    bucket_operation_args input_args = {hash_bucket_ptr, key, key_hash, NULL};

    data_node* data_node_ptr;
    int result = 0;
    result = g_hash_bucket_pool.is_concurrency_enabled ? _hash_bucket_lock_wrapper(FIND_NODE, input_args, &data_node_ptr) : _find_node(input_args, &data_node_ptr);

    if(result != 0) return result;
    return g_hash_bucket_pool.is_concurrency_enabled ? data_node_mutex_lock_wrapper(DATA_NODE_READ, data_node_ptr, value_out) : get_data_from_node(data_node_ptr, value_out);
}

int delete_node_from_bucket(unsigned int index, const char *key, uint32_t key_hash)
{
    if (key == NULL) return -20; // Error handling: invalid input
    
    hash_bucket *hash_bucket_ptr = get_hash_bucket(index);
    if (hash_bucket_ptr == NULL) return -40; // Error handling: bucket not found or initialized

    bucket_operation_args input_args = {hash_bucket_ptr, key, key_hash, NULL};

    data_node* data_node_ptr;

    return g_hash_bucket_pool.is_concurrency_enabled ? _hash_bucket_lock_wrapper(DELETE_NODE, input_args, &data_node_ptr) : _delete_node(input_args, &data_node_ptr);
}

void get_hash_bucket_pool_stats(keystore_stats* pool_out)
{
    if (pool_out == NULL) return;
    
    pool_out->key_entries = _calculate_key_entry_stats(&g_hash_bucket_pool);
    pool_out->collisions = _calculate_collision_stats(&g_hash_bucket_pool);
    pool_out->memory_pool = _calculate_memory_stats(&g_hash_bucket_pool, pool_out->key_entries.total_keys);
    pool_out->operation_counters = _get_operation_counters();
    pool_out->data_node_counters = get_data_node_operation_counters();
}

#pragma endregion

#pragma region Private Function Definitions

/**
 * @fn _is_power_of_two
 * @brief Checks if a given integer is a power of two.
 *
 * This function returns true if the input integer `n` is a power of two,
 * and false otherwise. It works for positive integers only.
 *
 * @param n The integer to check.
 * @return true if `n` is a power of two, false otherwise.
 */
static bool _is_power_of_two(unsigned int n) {
    return n > 0 && (n & (n - 1)) == 0;
}


/**
 * @fn _initialise_hash_bucket
 * @brief Initializes a hash bucket to its default state.
 *
 * This function sets the type of the hash bucket to BUCKET_LIST,
 * initializes its container to NULL, sets the count to 0, marks it
 * as initialized, and initializes the read-write lock for concurrency control.
 *
 * @param hash_bucket_ptr Pointer to the hash_bucket structure to be initialized.
 * @return int Returns 0 on success, or -1 on failure.
 */
static int _initialise_hash_bucket(hash_bucket *hash_bucket_ptr) {

    if (g_hash_bucket_pool.is_concurrency_enabled)
    {
        if (pthread_rwlock_init(&hash_bucket_ptr->lock, NULL) != 0) {
            return -11; // Error handling: lock initialization failed
        }
    }

    hash_bucket_ptr->type = BUCKET_LIST;
    hash_bucket_ptr->container.list = NULL;
    hash_bucket_ptr->count = 0;
    hash_bucket_ptr->is_initialized = true;
    
    return 0;
}

/**
 * @fn _delete_hash_bucket
 * @brief Cleans up and resets a hash bucket at the specified index.
 *
 * This function deletes all nodes in the hash bucket, frees associated memory,
 * and resets the bucket's properties to indicate it is uninitialized.
 *
 * @param index The index of the hash bucket to be deleted.
 */
static void _delete_hash_bucket(unsigned int index) {

    hash_bucket *hash_bucket_ptr = get_hash_bucket(index);
    if (hash_bucket_ptr == NULL) return; // Bucket not initialized, nothing to delete

    // Free resources based on bucket type
    switch (hash_bucket_ptr->type)
    {
        case BUCKET_LIST: 
            delete_all_list_nodes(hash_bucket_ptr->container.list);
            hash_bucket_ptr->container.list = NULL;
            break;
        default: return; // Error handling: unsupported bucket type
    }
    
    //reset properties of the hash bucket
    hash_bucket_ptr->type = NONE;
    hash_bucket_ptr->count = 0;
    hash_bucket_ptr->is_initialized = false;
    if (g_hash_bucket_pool.is_concurrency_enabled) pthread_rwlock_destroy(&hash_bucket_ptr->lock);
}

/**
 * @fn _add_node_to_bucket
 * @brief Adds a new node with the specified key and value to the hash bucket at the given index.
 *
 * This function creates a new data node and a corresponding list node,
 * then inserts the list node into the hash bucket's linked list.
 * If any step fails, it cleans up allocated resources and returns an error code.
 *
 * @param index The index of the hash bucket to which the node will be added.
 * @param key The key string for the new node.
 * @param key_hash The hash value of the key.
 * @param new_value Pointer to the key_store_value containing the data to be stored in the new node.
 * @return int Returns 0 on success, or a negative error code on failure.
 */
int _add_node_to_bucket(unsigned int index,  const char *key, uint32_t key_hash, key_store_value* new_value)
{
    if (key == NULL || new_value == NULL) return -20; // Error handling: invalid input

    // Get the target hash bucket
    hash_bucket *hash_bucket_ptr = get_hash_bucket(index);
    if (hash_bucket_ptr == NULL) return -40; // Error handling: bucket not found or initialized

    // Create new data node
    data_node* new_data_node = NULL;
    int create_result = create_data_node(key, key_hash, new_value, g_hash_bucket_pool.is_concurrency_enabled, &new_data_node);
    if (create_result != 0) return create_result; // Error handling: memory allocation failure

    // Create new list node
    list_node* new_list_node = create_new_list_node(key_hash, new_data_node);
    if (new_list_node == NULL) {
        delete_data_node(new_data_node);
        return -10; // Error handling: memory allocation failure
    }

    bucket_operation_args input_args = {hash_bucket_ptr, key, key_hash, new_list_node};

    int result = g_hash_bucket_pool.is_concurrency_enabled ? _hash_bucket_lock_wrapper(ADD_NODE, input_args , NULL) : _add_node(input_args);

    if (result != 0) {
        delete_data_node(new_data_node);
        free_memory(new_list_node, LIST_POOL);
    }

    return result;
}

#pragma endregion