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
#include "hash_buckets.h"
#include "hash_bucket_list.h"
#include "core/type_definition.h"
#include "core/data_node.h"
#include "utils/memory_manager.h"

#pragma region Private Global Variables
static hash_bucket_memory_pool g_hash_bucket_pool = {0};

#pragma endregion

#pragma region Private Type Definitions
typedef struct bucket_operation_args {
    hash_bucket *hash_bucket_ptr;
    const char *key;
    uint32_t key_hash;
    key_store_value* value;
} bucket_operation_args;

typedef enum {
    ADD_NODE,
    DELETE_NODE,
    FIND_NODE,
    EDIT_NODE
} bucket_operation_type_t;

#pragma endregion

#pragma region Private Function declarations

// Hash bucket utilities
static bool _is_power_of_two(unsigned int n);
static int _initialise_hash_bucket(hash_bucket *hash_bucket_ptr);
static data_node* _find_data_node(hash_bucket *hash_bucket_ptr, const char *key, uint32_t key_hash);

// Hash bucket operations
static void _delete_hash_bucket(unsigned int index);
static int _add_node(bucket_operation_args args);
static int _delete_node(bucket_operation_args args);
static int _find_node(bucket_operation_args args, key_store_value* value_out);
static int _edit_node(bucket_operation_args args);

// Concurrency control
static int _lock_wrapper(bucket_operation_type_t operation_type, bucket_operation_args args, key_store_value* value_out);
static int _mutex_lock_wrapper(bucket_operation_type_t operation_type, data_node* data_node_ptr, key_store_value* value_out);

#pragma endregion

#pragma region Public Function Definitions

int initialise_hash_buckets(unsigned int bucket_size, bool is_concurrency_enabled) 
{    
    if (!_is_power_of_two(bucket_size))  return -1; // Error handling: bucket_size must be a power of two

    if (g_hash_bucket_pool.is_initialized) return 0; // Already initialized

    g_hash_bucket_pool.block_size = sizeof(hash_bucket);
    g_hash_bucket_pool.is_initialized = false;
    g_hash_bucket_pool.total_blocks = bucket_size;

    g_hash_bucket_pool.hash_buckets_ptr = calloc(bucket_size, sizeof(hash_bucket));

    if (g_hash_bucket_pool.hash_buckets_ptr == NULL)  return -1; // Error handling: memory allocation failed

    g_hash_bucket_pool.is_initialized = true;
    g_hash_bucket_pool.is_concurrency_enabled = is_concurrency_enabled;

    // Eager initialization of hash buckets if concurrency is enabled or else lazy initialization will be done
    if (is_concurrency_enabled) {
        for (unsigned int i = 0; i < bucket_size; ++i) {
            if (_initialise_hash_bucket(&g_hash_bucket_pool.hash_buckets_ptr[i]) != 0) {
                cleanup_hash_buckets();
                return -1; // Error handling: failed to initialize hash bucket
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

int add_node_to_bucket(unsigned int index,  const char *key, uint32_t key_hash, key_store_value* new_value)
{
    hash_bucket *hash_bucket_ptr = get_hash_bucket(index);
    if (hash_bucket_ptr == NULL) return -1; // Error handling: bucket not found or initialized

    bucket_operation_args input_args = {hash_bucket_ptr, key, key_hash, new_value};

    return  g_hash_bucket_pool.is_concurrency_enabled ? _lock_wrapper(ADD_NODE, input_args , NULL) : _add_node(input_args);
}

int find_node_in_bucket(unsigned int index, const char *key, uint32_t key_hash, key_store_value* value_out)
{
    hash_bucket *hash_bucket_ptr = get_hash_bucket(index);
    if (hash_bucket_ptr == NULL) return -1; // Error handling: bucket not found or initialized

    bucket_operation_args input_args = {hash_bucket_ptr, key, key_hash, NULL};

    return g_hash_bucket_pool.is_concurrency_enabled ? _lock_wrapper(FIND_NODE, input_args, value_out) : _find_node(input_args, value_out);
}

int delete_node_from_bucket(unsigned int index, const char *key, uint32_t key_hash)
{
    hash_bucket *hash_bucket_ptr = get_hash_bucket(index);
    if (hash_bucket_ptr == NULL) return -1; // Error handling: bucket not found or initialized

    bucket_operation_args input_args = {hash_bucket_ptr, key, key_hash, NULL};

    return g_hash_bucket_pool.is_concurrency_enabled ? _lock_wrapper(DELETE_NODE, input_args, NULL) : _delete_node(input_args);
}

int edit_node_in_bucket(unsigned int index, const char *key, uint32_t key_hash, key_store_value* new_value)
{
    hash_bucket *hash_bucket_ptr = get_hash_bucket(index);
    if (hash_bucket_ptr == NULL) return -1; // Error handling: bucket not found or initialized

    bucket_operation_args input_args = {hash_bucket_ptr, key, key_hash, new_value};

    return g_hash_bucket_pool.is_concurrency_enabled ? _lock_wrapper(EDIT_NODE, input_args, NULL) : _edit_node(input_args);
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
bool _is_power_of_two(unsigned int n) {
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
int _initialise_hash_bucket(hash_bucket *hash_bucket_ptr) {

    if (g_hash_bucket_pool.is_concurrency_enabled)
    {
        if (pthread_rwlock_init(&hash_bucket_ptr->lock, NULL) != 0) {
            return -1; // Error handling: lock initialization failed
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
void _delete_hash_bucket(unsigned int index) {

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
 * @fn _add_node
 * @brief Adds a data node to the specified hash bucket.
 *
 * This function inserts a data node into the hash bucket's container
 * based on its type (currently only BUCKET_LIST is supported).
 *
 * @param args A struct containing the hash bucket, key hash, and data node to be added.
 * @return int Returns 0 on success, or -1 on failure.
 */
int _add_node(bucket_operation_args args)
{
    data_node *new_node = create_data_node(args.key, args.key_hash, args.value, g_hash_bucket_pool.is_concurrency_enabled);
    if (new_node == NULL) return -1; // Error handling: memory allocation failure
    
    int result = 0;
    switch (args.hash_bucket_ptr->type)
    {
        case BUCKET_LIST:
            result = insert_list_node(&args.hash_bucket_ptr->container.list, args.key_hash, new_node);
            break;
        default:
            result = -1; // Error handling: unsupported bucket type
            break;
    }

    if(result == 0) {
        args.hash_bucket_ptr->count += 1;
    } else {
        delete_data_node(new_node); // Clean up allocated node on failure
    }

    return result;
}

/**
 * @fn _delete_node
 * @brief Deletes a data node from the specified hash bucket.
 *
 * This function removes a data node from the hash bucket's container
 * based on its type (currently only BUCKET_LIST is supported).
 *
 * @param args A struct containing the hash bucket, key hash, and key of the node to be deleted.
 * @return int Returns 0 on success, or -1 on failure.
 */
int _delete_node(bucket_operation_args args)
{
    int result = 0;
    switch (args.hash_bucket_ptr->type)
    {
        case BUCKET_LIST:
            result = delete_list_node(&args.hash_bucket_ptr->container.list, args.key, args.key_hash);
            break;
        default:
            result = -1; // Error handling: unsupported bucket type
            break;
    }

    if(result == 0) {
        args.hash_bucket_ptr->count -= 1;
    }

    return result;
}

/**
 * @fn _find_data_node
 * @brief Finds a data node in the specified hash bucket.
 *
 * This function searches for a data node in the hash bucket's container
 * based on its type (currently only BUCKET_LIST is supported).
 *
 * @param hash_bucket_ptr Pointer to the hash bucket to search in.
 * @param key The key string of the node to find.
 * @param key_hash Hash value of the key.
 * @return Pointer to the found data node, or NULL if not found.
 */
data_node* _find_data_node(hash_bucket *hash_bucket_ptr, const char *key, uint32_t key_hash)
{
    data_node* data_node_ptr = NULL;
    
    // Find the data node based on bucket type
    switch (hash_bucket_ptr->type)
    {
        case BUCKET_LIST:
            list_node* found_node = find_list_node(hash_bucket_ptr->container.list, key, key_hash);
            data_node_ptr = (found_node != NULL) ? found_node->data : NULL;
            break;
        default: 
            return NULL; // Error handling: unsupported bucket type
    }

    return data_node_ptr;
}

/**
 * @fn _find_node
 * @brief Finds a data node in the specified hash bucket and retrieves its value.
 *
 * This function searches for a data node in the hash bucket's container
 * based on its type (currently only BUCKET_LIST is supported) and copies
 * its data to the output parameter if found.
 *
 * @param args A struct containing the hash bucket, key hash, and key of the node to be found.
 * @param value_out Pointer to a key_store_value structure to receive the found value.
 * @return int Returns 0 on success, or -1 if the node was not found or on error.
 */
int _find_node(bucket_operation_args args, key_store_value* value_out)
{
    // Find the data node
    data_node* data_node_ptr = _find_data_node(args.hash_bucket_ptr, args.key, args.key_hash);
    if(data_node_ptr == NULL)  return -1; //Error handling: node not found

    return (g_hash_bucket_pool.is_concurrency_enabled) ? _mutex_lock_wrapper(FIND_NODE, data_node_ptr, value_out) : get_data_from_node(data_node_ptr, value_out);
}

/**
 * @fn _edit_node
 * @brief Edits the data of a node in the specified hash bucket.
 *
 * This function searches for a data node in the hash bucket's container
 * based on its type (currently only BUCKET_LIST is supported) and updates
 * its data if found.
 *
 * @param args A struct containing the hash bucket, key hash, key, and new value for the node to be edited.
 * @return int Returns 0 on success, or -1 if the node was not found or on error.
 */
int _edit_node(bucket_operation_args args)
{
    data_node* data_node_ptr = _find_data_node(args.hash_bucket_ptr, args.key, args.key_hash);
    if(data_node_ptr == NULL)  return -1; //Error handling: node not found

    return (g_hash_bucket_pool.is_concurrency_enabled) ? _mutex_lock_wrapper(EDIT_NODE, data_node_ptr, args.value) : update_data_node(data_node_ptr, args.value);
}

/**
 * @fn _lock_wrapper
 * @brief Wraps bucket operations with read-write lock for concurrency control.
 *
 * This function acquires the appropriate lock (read or write) based on the
 * operation type, performs the operation, and then releases the lock.
 *
 * @param operation_type The type of operation to perform (ADD_NODE, DELETE_NODE, FIND_NODE, EDIT_NODE).
 * @param args A struct containing the hash bucket and operation parameters.
 * @param result_out Pointer to a key_store_value structure to receive the result for FIND_NODE operations.
 * @return int Returns the result of the operation, or -1 on lock acquisition failure.
 */
int _lock_wrapper(bucket_operation_type_t operation_type, bucket_operation_args args, key_store_value *result_out) {
    
    int lock_result = 0;
    
    if (operation_type == FIND_NODE) {
        lock_result = pthread_rwlock_rdlock(&args.hash_bucket_ptr->lock);
    } else {
        lock_result = pthread_rwlock_wrlock(&args.hash_bucket_ptr->lock);
    }

    if (lock_result != 0) return -1; // Handle error: failed to acquire lock

    int operation_result = 0;
    switch (operation_type) {
        case ADD_NODE:
            operation_result = _add_node(args);
            break;
        case DELETE_NODE:
            operation_result = _delete_node(args);
            break;
        case FIND_NODE:
            operation_result = _find_node(args, result_out);
            break;
        case EDIT_NODE:
            operation_result = _edit_node(args);
            break;
        default: 
            // Error handling: unknown operation
            operation_result = -41;
            break;
    }

    pthread_rwlock_unlock(&args.hash_bucket_ptr->lock);
    return operation_result;
}

/**
 * @fn _mutex_lock_wrapper
 * @brief Wraps data node operations with mutex lock for concurrency control.
 *
 * This function acquires the mutex lock of the data node, performs the
 * operation (FIND_NODE or EDIT_NODE), and then releases the lock.
 *
 * @param operation_type The type of operation to perform (FIND_NODE or EDIT_NODE).
 * @param data_node_ptr Pointer to the data node on which to perform the operation.
 * @param value Pointer to a key_store_value structure for input/output as needed.
 * @return int Returns the result of the operation, or -1 on lock acquisition failure.
 */
int _mutex_lock_wrapper(bucket_operation_type_t operation_type, data_node* data_node_ptr, key_store_value* value) {
    int lock_result = pthread_mutex_lock(&data_node_ptr->lock);
    if (lock_result != 0) return -1; // Handle error: failed to acquire lock

    int operation_result = 0;
    switch (operation_type) {
        case FIND_NODE:
            operation_result = get_data_from_node(data_node_ptr, value);
            break;
        case EDIT_NODE:
            operation_result = update_data_node(data_node_ptr, value);
            break;
        default: 
            // Error handling: unknown operation
            operation_result = -1;
            break;
    }

    pthread_mutex_unlock(&data_node_ptr->lock);
    return operation_result;
}

#pragma endregion