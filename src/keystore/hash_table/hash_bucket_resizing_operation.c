#include "hash_bucket_resizing_operation.h"
#include "data_structures/data_node_operation.h"
#include "data_structures/linked_list_operation.h"
#include "sub_hash_table/sub_hash_bucket_operation.h"
#include "sub_hash_table/sub_hash_table_operation.h"
#include "utils/helper_functions.h"
#include <string.h>

#pragma region Resizing Helper Functions Declarations
static int _apply_pending_list_operations_to_sub_hash_table(hash_bucket* hash_bucket_ptr, sub_hash_table_memory_pool* target_sub_hash_table_ptr);
static int _append_list_nodes_to_sub_hash_table(linked_list_node* source_linked_list_head, sub_hash_table_memory_pool* sub_hash_table_ptr);
static int _fillup_new_sub_hash_table(hash_bucket* hash_bucket_ptr, sub_hash_table_memory_pool* new_sub_hash_table_ptr);
static int _perform_hash_bucket_resizing(hash_bucket* hash_bucket_ptr, sub_hash_table_configuration new_config, int retry_count, sub_hash_table_memory_pool** new_sub_hash_table_out);
static int _finalize_hash_bucket_resizing(hash_bucket* hash_bucket_ptr, sub_hash_table_configuration new_config, sub_hash_table_memory_pool* new_sub_hash_table_ptr);
#pragma endregion

#pragma region Private Function Definitions
int _find_node_in_hash_bucket_during_resizing(hash_bucket* hash_bucket_ptr, const char* key, uint32_t key_hash, key_value_pair* key_value_pair_out);
int _add_node_to_pending_list(hash_bucket* hash_bucket_ptr, uint32_t key_hash, key_value_pair* kv_pair, bool is_delete_operation);
int _resize_hash_bucket(void* hash_bucket_ptr);
int _delete_node_while_resizing(hash_bucket* hash_bucket_ptr, const char *key, uint32_t key_hash);
int _update_node_while_resizing(hash_bucket* hash_bucket_ptr, uint32_t key_hash, key_value_pair* kv_pair);
#pragma endregion


#pragma region Public Function Definitions

int initialize_hash_bucket_resizing(hash_bucket* hash_bucket_ptr)
{
    if (hash_bucket_ptr == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input

    hash_bucket_ptr->snapshot_sub_hash_table_ptr = NULL;
    hash_bucket_ptr->is_resizing = true;
    hash_bucket_ptr->snapshot_sub_hash_table_ptr = hash_bucket_ptr->sub_hash_table_ptr;
    hash_bucket_ptr->sub_hash_table_ptr = NULL; // New sub-hash-table will be assigned later

    return initialize_background_function(_resize_hash_bucket, (void*)hash_bucket_ptr);
}

int upsert_node_to_hash_bucket_during_resizing(hash_bucket* hash_bucket_ptr, uint32_t key_hash, key_value_pair* kv_pair) {
    if (hash_bucket_ptr == NULL || kv_pair == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input

    //If the resizing finished while waiting for the lock, proceed with normal upsert
    if(!hash_bucket_ptr->is_resizing) {
        return upsert_node_to_sub_hash_table(hash_bucket_ptr->sub_hash_table_ptr, key_hash, kv_pair);
    }

    return _update_node_while_resizing(hash_bucket_ptr, key_hash, kv_pair);
}

int get_key_value_from_hash_bucket_during_resizing(hash_bucket* hash_bucket_ptr, const char *key, uint32_t key_hash, key_value_pair* kv_pair_out) {
    if (hash_bucket_ptr == NULL || kv_pair_out == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input

    //If the resizing finished while waiting for the lock, proceed with normal upsert
    if(!hash_bucket_ptr->is_resizing) {
        return get_key_store_value_from_sub_hash_table(hash_bucket_ptr->sub_hash_table_ptr, key_hash, key, kv_pair_out);
    }

    return _find_node_in_hash_bucket_during_resizing(hash_bucket_ptr, key, key_hash, kv_pair_out);
}

int delete_key_from_hash_bucket_during_resizing(hash_bucket* hash_bucket_ptr, const char *key, uint32_t key_hash) {
    if (hash_bucket_ptr == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input

    //If the resizing finished while waiting for the lock, proceed with normal upsert
    if(!hash_bucket_ptr->is_resizing) {
        return delete_key_from_sub_hash_table(hash_bucket_ptr->sub_hash_table_ptr, key_hash, key);
    }

    return _delete_node_while_resizing(hash_bucket_ptr, key, key_hash);
}

#pragma endregion

#pragma region Private Function Definitions

int _find_node_in_hash_bucket_during_resizing(hash_bucket* hash_bucket_ptr, const char* key, uint32_t key_hash, key_value_pair* key_value_pair_out)
{
    if (hash_bucket_ptr == NULL || key_value_pair_out == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input

    int result = 0;
    
    // First, check in the pending list
    data_node* target_data_node = NULL;
    result = get_data_node_from_linked_list(hash_bucket_ptr->pending_list_head, key, key_hash, true, &target_data_node);

    if (result == SUCCESS) {

        // Check if the node is marked as deleted
        if (target_data_node->is_deleted) {
            return ERR_DATA_NODE_NOT_FOUND; // Node is marked as deleted
        }
        else{

            return read_data_node_value(target_data_node, key_value_pair_out);
        }

    }

    // Next, check in the snapshot sub-hash-table
    if (hash_bucket_ptr->snapshot_sub_hash_table_ptr != NULL) {
        result = get_key_store_value_from_sub_hash_table(hash_bucket_ptr->snapshot_sub_hash_table_ptr, key_hash, key, key_value_pair_out);
        if (result == SUCCESS) {
            return SUCCESS; // Node found in snapshot sub-hash-table
        }
    }

    return ERR_DATA_NODE_NOT_FOUND; // Node not found
}

int _add_node_to_pending_list(hash_bucket* hash_bucket_ptr, uint32_t key_hash, key_value_pair* kv_pair, bool is_delete_operation)
{
    if (hash_bucket_ptr == NULL || kv_pair == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input

    data_node* new_data_node = NULL;
    int result = create_new_data_node(key_hash, kv_pair, hash_bucket_ptr->sub_hash_table_config.is_concurrency_enabled, &new_data_node);
    if (result != SUCCESS) return result; // Error handling: failed to create new data node
    
    // Mark as deleted if it's a delete operation
    if(is_delete_operation) {
        new_data_node->is_deleted = true;
    }

    linked_list_node* new_list_node = NULL;
    result = create_new_linked_list_node(key_hash, new_data_node, &new_list_node);
    if (result != SUCCESS) {
        delete_data_node(new_data_node);
        return ERR_MEMORY_ALLOCATION_FAILED; // Error handling: memory allocation failure
    }

    result = insert_linked_list_node(&hash_bucket_ptr->pending_list_head, new_list_node);

    return (result == SUCCESS) ? SUCCESS_ADDED_TO_PENDING_LIST : result; // Return SUCCESS_ADDED_TO_PENDING_LIST to indicate node added to pending list successfully
}


int _resize_hash_bucket(void* input_arg)
{
    if (input_arg == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input

    hash_bucket* hash_bucket_ptr = (hash_bucket*)input_arg;
    
    sub_hash_table_configuration new_config = {
        .is_concurrency_enabled = hash_bucket_ptr->sub_hash_table_config.is_concurrency_enabled,
        .bucket_size = hash_bucket_ptr->sub_hash_table_config.bucket_size * 2, // Double the bucket size
        .max_linked_list_chain_length = hash_bucket_ptr->sub_hash_table_config.max_linked_list_chain_length
    };

    sub_hash_table_memory_pool* new_sub_hash_table_ptr = NULL;
    _perform_hash_bucket_resizing(hash_bucket_ptr, new_config, 3, &new_sub_hash_table_ptr);

    //acquire resizing lock
    pthread_mutex_lock(&hash_bucket_ptr->resizing_lock);

    _finalize_hash_bucket_resizing(hash_bucket_ptr, new_config, new_sub_hash_table_ptr);

    pthread_mutex_unlock(&hash_bucket_ptr->resizing_lock);

    return SUCCESS;
}

int _delete_node_while_resizing(hash_bucket* hash_bucket_ptr, const char *key, uint32_t key_hash)
{
    int result = 0;
    data_node* target_data_node = NULL;
    result = get_data_node_from_linked_list(hash_bucket_ptr->pending_list_head, key, key_hash, false, &target_data_node);
    
    if(result == SUCCESS) {
        // Node found, update it
        int delete_result = soft_delete_data_node(target_data_node);
        return delete_result;
    }
    else if(result == ERR_DATA_NODE_NOT_FOUND) {
        // Node not found, add to pending list
        // key_value_pair with dummy value indicates delete operation
        char* dummy_value = "to_be_deleted";
        key_value_pair kv_pair = {.key = (char*)key, .value = (unsigned char *)dummy_value, .value_size = strlen(dummy_value)+1};
        return _add_node_to_pending_list(hash_bucket_ptr, key_hash, &kv_pair, true);
    }
    else {
        return result; // Propagate other errors
    }
}

int _update_node_while_resizing(hash_bucket* hash_bucket_ptr, uint32_t key_hash, key_value_pair* kv_pair)
{
    int result = 0;
    data_node* target_data_node = NULL;
    result = get_data_node_from_linked_list(hash_bucket_ptr->pending_list_head, kv_pair->key, key_hash, false, &target_data_node);
    
    // If found, update the existing node. else, add to pending list
    if(result == SUCCESS) {
        return edit_data_node_value(target_data_node, kv_pair);
    }
    else if(result == ERR_DATA_NODE_NOT_FOUND) {
        return _add_node_to_pending_list(hash_bucket_ptr, key_hash, kv_pair, false);
    }
    else {
        return result;
    }
}

#pragma endregion

#pragma region Resizing Helper Function Definitions

/**
 * @fn perform_hash_bucket_resizing
 * @brief Performs the resizing of the hash bucket by creating a new sub-hash-table and populating it with existing data.
 * @param hash_bucket_ptr Pointer to the hash bucket to be resized.
 * @param new_config Configuration for the new sub-hash-table.
 * @param retry_count Number of retry attempts in case of failure.
 * @param new_sub_hash_table_out Output pointer to the newly created sub-hash-table.
 * @return int SUCCESS on success, error code on failure.
 */
int _perform_hash_bucket_resizing(hash_bucket* hash_bucket_ptr, sub_hash_table_configuration new_config, int retry_count, sub_hash_table_memory_pool** new_sub_hash_table_out)
{
    if (hash_bucket_ptr == NULL || new_sub_hash_table_out == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input
    if (retry_count <= 0) return ERR_MAX_RETRY_EXCEEDED; // Error handling: max retries exceeded

    int result = 0;
    
    for (int attempt = 0; attempt < retry_count; ++attempt) {
        sub_hash_table_memory_pool* new_sub_hash_table_ptr = NULL;
        result = create_new_sub_hash_table(new_config, true, &new_sub_hash_table_ptr);
        if (result == SUCCESS) {
            result = _fillup_new_sub_hash_table(hash_bucket_ptr, new_sub_hash_table_ptr);
            if (result == SUCCESS) {
               *new_sub_hash_table_out = new_sub_hash_table_ptr;
               return SUCCESS;
            }
        }

        if(new_sub_hash_table_ptr != NULL) {
            cleanup_sub_hash_table(new_sub_hash_table_ptr);
            new_sub_hash_table_ptr = NULL;
        }

        continue;
    }

    new_sub_hash_table_out = NULL;
    return result; // Return the last error encountered
}

/**
 * @fn fillup_new_sub_hash_table
 * @brief Fills up the new sub-hash-table with data from the snapshot sub-hash-table. During this process, it iterates through each sub-bucket
 * of the snapshot sub-hash-table, and for each initialized sub-bucket, it traverses its linked list to append nodes to the new sub-hash-table.
 * It skips uninitialized sub-buckets and empty linked lists to optimize the process.
 * @param hash_bucket_ptr Pointer to the hash bucket containing the snapshot sub-hash-table.
 * @param new_sub_hash_table_ptr Pointer to the new sub-hash-table to be filled.
 * @return int SUCCESS on success, error code on failure.
*/
int _fillup_new_sub_hash_table(hash_bucket* hash_bucket_ptr, sub_hash_table_memory_pool* new_sub_hash_table_ptr)
{
    if (hash_bucket_ptr == NULL || new_sub_hash_table_ptr == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input

    if (hash_bucket_ptr->snapshot_sub_hash_table_ptr == NULL) return ERR_SUB_HASH_TABLE_NOT_INITIALIZED;

    for(unsigned int i = 0; i < hash_bucket_ptr->snapshot_sub_hash_table_ptr->block_size; ++i) {
        sub_hash_bucket* current_sub_bucket = &hash_bucket_ptr->snapshot_sub_hash_table_ptr->sub_hash_buckets_ptr[i];

        // Skip uninitialized sub-buckets
        if(current_sub_bucket == NULL || !current_sub_bucket->is_initialized)
        {
            continue; 
        }

        linked_list_node* current_header = current_sub_bucket->linked_list_head;

        // Skip empty linked lists
        if(current_header == NULL) {
            continue;
        }
        
        int result = _append_list_nodes_to_sub_hash_table(current_header, new_sub_hash_table_ptr);

        //Propagate error if any, to retry the resizing
        if(result != SUCCESS) {
            return result;
        }
    }

    return SUCCESS;
}

/**
 * @fn finalize_hash_bucket_resizing
 * @brief Finalizes the resizing of the hash bucket by applying pending operations and swapping in the new sub-hash-table. It applies, all the pending
 * operations recorded during the resizing process to the new sub-hash-table or the snapshot sub-hash-table if no new one was created. After successfully applying
 * the pending operations, it swaps in the new sub-hash-table as the active one for the hash bucket. Finally, it cleans up the pending list and resets the resizing state.
 * @param hash_bucket_ptr Pointer to the hash bucket being resized.
 * @param new_config Configuration for the new sub-hash-table.
 * @param new_sub_hash_table_ptr Pointer to the newly created sub-hash-table.
 * @return int SUCCESS on success, error code on failure.
 */
int _finalize_hash_bucket_resizing(hash_bucket* hash_bucket_ptr, sub_hash_table_configuration new_config, sub_hash_table_memory_pool* new_sub_hash_table_ptr)
{
    if (hash_bucket_ptr == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input

    int result = 0;
    if(new_sub_hash_table_ptr != NULL)
    {
        result = _apply_pending_list_operations_to_sub_hash_table(hash_bucket_ptr, new_sub_hash_table_ptr);
    }
    else {
        result = _apply_pending_list_operations_to_sub_hash_table(hash_bucket_ptr, hash_bucket_ptr->snapshot_sub_hash_table_ptr);
    }

    // Propagate error if any, to retry the resizing
    if(result != SUCCESS) {
        return result;
    }

    //Swap in the new sub-hash-table
    if(new_sub_hash_table_ptr != NULL) {
        hash_bucket_ptr->sub_hash_table_ptr = new_sub_hash_table_ptr;
        hash_bucket_ptr->sub_hash_table_config = new_config;
        if(hash_bucket_ptr->snapshot_sub_hash_table_ptr != NULL) {
            cleanup_sub_hash_table(hash_bucket_ptr->snapshot_sub_hash_table_ptr);
            hash_bucket_ptr->snapshot_sub_hash_table_ptr = NULL;
        }
    }
    else {
        //If no new sub-hash-table was provided, retain the old one
        hash_bucket_ptr->sub_hash_table_ptr = hash_bucket_ptr->snapshot_sub_hash_table_ptr;
    }

    if(result == SUCCESS) {
        delete_all_linked_list_nodes(hash_bucket_ptr->pending_list_head);
        hash_bucket_ptr->pending_list_head = NULL;
    }

    hash_bucket_ptr->is_resizing = false;
    hash_bucket_ptr->snapshot_sub_hash_table_ptr = NULL;
    return SUCCESS;
}

/**
 * @fn append_list_nodes_to_sub_hash_table
 * @brief Appends nodes from a linked list to a sub-hash-table. It iterates through each node in the provided linked list,
 * and for each node that is not marked as deleted, it upserts the corresponding key-value pair into the specified sub-hash-table.
 * @param source_linked_list_head Pointer to the head of the source linked list.
 * @param sub_hash_table_ptr Pointer to the sub-hash-table where nodes will be appended.
 * @return int SUCCESS on success, error code on failure.
 */
int _append_list_nodes_to_sub_hash_table(linked_list_node* source_linked_list_head, sub_hash_table_memory_pool* sub_hash_table_ptr)
{
    if (sub_hash_table_ptr == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input

    linked_list_node* current_node = source_linked_list_head;
    
    while (current_node != NULL) {

        if (current_node->data_node_ptr == NULL) {
            current_node = current_node->next_node_ptr;
            continue; // Skip nodes with NULL data_node_ptr
        }

        if(current_node->data_node_ptr-> is_deleted) {
            current_node = current_node->next_node_ptr;
            continue; // Skip deleted nodes
        }
        
        key_value_pair kv_pair = {
            .key = current_node->data_node_ptr->key,
            .value = current_node->data_node_ptr->data,
            .value_size = current_node->data_node_ptr->data_size
        };

        int result = upsert_node_to_sub_hash_table(sub_hash_table_ptr, current_node->key_hash, &kv_pair);
        if (result != SUCCESS) {
            return result; // Propagate error
        }

        current_node = current_node->next_node_ptr;
    }

    return SUCCESS;
}

/**
 * @fn apply_pending_list_operations_to_sub_hash_table
 * @brief Applies pending operations recorded during resizing to the specified sub-hash-table. It iterates through each node
 * in the pending list of the hash bucket, and for each node, it checks whether it is marked as deleted or not. If the node is marked
 * as deleted, it performs a delete operation on the target sub-hash-table. If the node is not marked as deleted, it performs an upsert
 * operation to add or update the corresponding key-value pair in the target sub-hash-table.
 * @param hash_bucket_ptr Pointer to the hash bucket containing the pending list.
 * @param target_sub_hash_table_ptr Pointer to the target sub-hash-table where operations will be applied.
 * @return int SUCCESS on success, error code on failure.
 */
int _apply_pending_list_operations_to_sub_hash_table(hash_bucket* hash_bucket_ptr, sub_hash_table_memory_pool* target_sub_hash_table_ptr)
{
    if (hash_bucket_ptr == NULL || target_sub_hash_table_ptr == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input

    linked_list_node* current_node = hash_bucket_ptr->pending_list_head;
    
    while (current_node != NULL) {

        if(current_node->data_node_ptr-> is_deleted) {
            // Delete operation
            int delete_result = delete_key_from_sub_hash_table(target_sub_hash_table_ptr, current_node->key_hash, current_node->data_node_ptr->key);
            if (delete_result != SUCCESS && delete_result != ERR_DATA_NODE_NOT_FOUND) {
                return delete_result; // Propagate error
            }
        }
        else {
            // Upsert operation
            key_value_pair kv_pair = {
                .key = current_node->data_node_ptr->key,
                .value = current_node->data_node_ptr->data,
                .value_size = current_node->data_node_ptr->data_size
            };
            int upsert_result = upsert_node_to_sub_hash_table(target_sub_hash_table_ptr, current_node->key_hash, &kv_pair);
            if (upsert_result != SUCCESS && upsert_result != SUCESS_ADDED_NEW_NODE_RESZING_TRIGGERED) {
                return upsert_result; // Propagate error
            }
        }

        current_node = current_node->next_node_ptr;
    }

    return SUCCESS;
}

#pragma endregion