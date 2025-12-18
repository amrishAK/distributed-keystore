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
    list_node* new_list_node;
} bucket_operation_args;

typedef enum {
    ADD_NODE,
    DELETE_NODE,
    FIND_NODE
} bucket_operation_type_t;

#pragma endregion


#pragma region Private Function declarations
// Helper to find data node in bucket
static data_node* _find_data_node(hash_bucket *hash_bucket_ptr, const char *key, uint32_t key_hash);

// Stat helpers
static int _operation_counter_increment(bucket_operation_type_t operation_type, int operation_result);

#pragma endregion


#pragma region Private Global Variables
static bucket_operation_counter_stats g_operation_counters = {0};
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
bucket_operation_counter_stats _get_operation_counters(void)
{
    bucket_operation_counter_stats counters_copy;
    memcpy(&counters_copy, &g_operation_counters, sizeof(bucket_operation_counter_stats));
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
            g_operation_counters.total_find_ops++;
            if (operation_result != 0) g_operation_counters.failed_find_ops++;
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
 * This function inserts a data node into the hash bucket's container and increments its count after successful addition
 * based on its type (currently only BUCKET_LIST is supported).
 *
 * @param args A struct containing the hash bucket, key hash, and list node to be added.
 * @note The caller is responsible for creating the list_node with associated data_node. 
 * @note The caller is responsible for managing the memory of the list_node and data_node in case of failure.
 * @return int Returns 0 on success, or -1 on failure.
 */
int _add_node(bucket_operation_args args)
{
    int result = 0;
    switch (args.hash_bucket_ptr->type)
    {
        case BUCKET_LIST:
            result = insert_list_node(&args.hash_bucket_ptr->container.list, args.new_list_node);
            break;
        default:
            result = -43; // Error handling: unsupported bucket type
            break;
    }

    if(result == 0) {
        args.hash_bucket_ptr->count += 1;
    }

    return _operation_counter_increment(ADD_NODE, result);
}

/**
 * @fn _delete_node
 * @brief Deletes a data node from the specified hash bucket.
 * This function removes a data node from the hash bucket's container
 * based on its type (currently only BUCKET_LIST is supported) and decrements its count after successful deletion.
 * 
 * @param args A struct containing the hash bucket, key hash, and key of the node to be deleted.
 * @param deleted_node_out Pointer to a data_node pointer to receive the deleted node.
 * @note The caller is responsible for managing the memory of the deleted data_node.
 * @return int Returns 0 on success, or -1 on failure.
 */
int _delete_node(bucket_operation_args args, data_node** deleted_node_out)
{
    int result = 0;
    switch (args.hash_bucket_ptr->type)
    {
        case BUCKET_LIST:
            result = delete_list_node(&args.hash_bucket_ptr->container.list, args.key, args.key_hash, deleted_node_out);
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
int _find_node(bucket_operation_args args, data_node** data_node_out)
{
    int result = 0;

    // Find the data node
    data_node* data_node_ptr = _find_data_node(args.hash_bucket_ptr, args.key, args.key_hash);
    result = (data_node_ptr != NULL) ? 0 : -41;

    *data_node_out = data_node_ptr;
    
    return _operation_counter_increment(FIND_NODE, result);
}

#pragma endregion

#pragma region Concurrency Control Definitions

/**
 * @fn _hash_bucket_lock_wrapper
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
int _hash_bucket_lock_wrapper(bucket_operation_type_t operation_type, bucket_operation_args args, data_node** data_node_out) {
    
    int lock_result = 0;
    
    if (operation_type == FIND_NODE) {
        lock_result = pthread_rwlock_rdlock(&args.hash_bucket_ptr->lock);
    } else {
        lock_result = pthread_rwlock_wrlock(&args.hash_bucket_ptr->lock);
    }

    if (lock_result != 0) return _operation_counter_increment(operation_type, -30); // Handle error: failed to acquire lock

    int operation_result = 0;
    switch (operation_type) {
        case ADD_NODE:
            operation_result = _add_node(args);
            break;
        case DELETE_NODE:
            operation_result = _delete_node(args, data_node_out);
            break;
        case FIND_NODE:
            operation_result = _find_node(args, data_node_out);
            break;
        default: 
            // Error handling: unknown operation
            operation_result = -43;
            break;
    }

    if (pthread_rwlock_unlock(&args.hash_bucket_ptr->lock) != 0) return _operation_counter_increment(operation_type, -31); // Handle error: failed to release lock
    return _operation_counter_increment(operation_type, operation_result);
}

#pragma endregion
