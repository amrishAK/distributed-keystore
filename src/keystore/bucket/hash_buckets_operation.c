#include <string.h>
#include <stdlib.h>

#include "core/type_definition.h"
#include "core/data_node.h"
#include "hash_bucket_list.h"

#pragma region Private Type Definitions
typedef struct {
    hash_bucket *hash_bucket_ptr;
    const char *key;
    uint32_t key_hash;
    key_store_value* value;
    bool is_concurrency_enabled;
} bucket_operation_args;

typedef enum {
    ADD_NODE,
    DELETE_NODE,
    FIND_NODE,
    EDIT_NODE,
    GET_STATS
} bucket_operation_type_t;

#pragma endregion


#pragma region Private Function declarations
// Helper to find data node in bucket
static data_node* _find_data_node(hash_bucket *hash_bucket_ptr, const char *key, uint32_t key_hash);

// Mutex lock wrapper
static int _mutex_lock_wrapper(bucket_operation_type_t operation_type, data_node* data_node_ptr, key_store_value* value_out);

// Stat helpers
static int _operation_counter_increment(bucket_operation_type_t operation_type, int operation_result);

#pragma endregion


#pragma region Private Global Variables
static operation_counter_stats g_operation_counters = {0};
#pragma endregion


#pragma region Statistics Function Definitions

/**
 * @fn _get_operation_counters
 * @brief Retrieves a copy of the global operation counters.
 *
 * This function returns a copy of the global operation_counter_stats structure,
 * which contains counts of total and failed operations as well as error code occurrences.
 *
 * @return operation_counter_stats A copy of the global operation counters.
 */
operation_counter_stats _get_operation_counters(void)
{
    operation_counter_stats counters_copy;
    memcpy(&counters_copy, &g_operation_counters, sizeof(operation_counter_stats));
    return counters_copy;
}

/**
 * @fn _operation_counter_increment
 * @brief Increments the operation counters based on the operation type and result.
 *
 * This function updates the global operation counters for add, delete, find, and edit operations.
 * It also increments the failed operation counters and error code counters based on the result.
 *
 * @param operation_type The type of operation performed.
 * @param operation_result The result of the operation (0 for success, negative for errors).
 * @return int The operation_result passed in.
 */
static int _operation_counter_increment(bucket_operation_type_t operation_type, int operation_result)
{
    switch (operation_type)
    {
        case ADD_NODE:
            g_operation_counters.total_add_ops++;
            if (operation_result != 0) g_operation_counters.failed_add_ops++;
            break;
        case DELETE_NODE:
            g_operation_counters.total_delete_ops++;
            if (operation_result != 0) g_operation_counters.failed_delete_ops++;
            break;
        case FIND_NODE:
            g_operation_counters.total_get_ops++;
            if (operation_result != 0) g_operation_counters.failed_get_ops++;
            break;
        case EDIT_NODE:
            g_operation_counters.total_edit_ops++;
            if (operation_result != 0) g_operation_counters.failed_edit_ops++;
            break;
        default:
            break;
    }

    if (operation_result < 0 && operation_result > -100) {
        g_operation_counters.error_code_counters[-operation_result]++;
    }

    return operation_result;
}

#pragma endregion

#pragma region Bucket Operation Definitions

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
    data_node *new_node = create_data_node(args.key, args.key_hash, args.value, args.is_concurrency_enabled);
    if (new_node == NULL) return _operation_counter_increment(ADD_NODE, -10); // Error handling: memory allocation failure
    
    int result = 0;
    switch (args.hash_bucket_ptr->type)
    {
        case BUCKET_LIST:
            result = insert_list_node(&args.hash_bucket_ptr->container.list, args.key_hash, new_node);
            break;
        default:
            result = -43; // Error handling: unsupported bucket type
            break;
    }

    if(result == 0) {
        args.hash_bucket_ptr->count += 1;
    } else {
        delete_data_node(new_node); // Clean up allocated node on failure
    }

    return _operation_counter_increment(ADD_NODE, result);
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
            result = -43; // Error handling: unsupported bucket type
            break;
    }

    if(result == 0) {
        args.hash_bucket_ptr->count -= 1;
    }

    return _operation_counter_increment(DELETE_NODE, result);
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
    if(data_node_ptr == NULL)  return _operation_counter_increment(FIND_NODE, -41); //Error handling: node not found
    
    int result = 0;
    result = (args.is_concurrency_enabled) ? _mutex_lock_wrapper(FIND_NODE, data_node_ptr, value_out) : get_data_from_node(data_node_ptr, value_out);
    
    return _operation_counter_increment(FIND_NODE, result);
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
    if(data_node_ptr == NULL)  return _operation_counter_increment(EDIT_NODE, -41); //Error handling: node not found

    int result = 0;
    result = (args.is_concurrency_enabled) ? _mutex_lock_wrapper(EDIT_NODE, data_node_ptr, args.value) : update_data_node(data_node_ptr, args.value);
    return _operation_counter_increment(EDIT_NODE, result);
}

#pragma endregion

#pragma region Concurrency Control Definitions

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

    if (lock_result != 0) return -30; // Handle error: failed to acquire lock

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
            operation_result = -47;
            break;
    }

    if (pthread_rwlock_unlock(&args.hash_bucket_ptr->lock) != 0) return -31; // Handle error: failed to release lock
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
    if (lock_result != 0) return -30; // Handle error: failed to acquire lock

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
            operation_result = -47;
            break;
    }

    if (pthread_mutex_unlock(&data_node_ptr->lock) != 0) return -31; // Handle error: failed to release lock
    return operation_result;
}

#pragma endregion
