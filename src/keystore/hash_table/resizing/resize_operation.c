#include "resize_operation.h"
#include "sub_hash_table/sub_hash_table_operation.h"
#include "data_structures/linked_list_operation.h"

#pragma region Resizing Helper Functions Declarations
static int _apply_pending_list_operations_to_sub_hash_table(hash_bucket* hash_bucket_ptr, sub_hash_table_memory_pool* target_sub_hash_table_ptr);
static int _append_list_nodes_to_sub_hash_table(linked_list_node* source_linked_list_head, sub_hash_table_memory_pool* sub_hash_table_ptr);
static int _fillup_new_sub_hash_table(hash_bucket* hash_bucket_ptr, sub_hash_table_memory_pool* new_sub_hash_table_ptr);
static int _perform_hash_bucket_resizing(hash_bucket* hash_bucket_ptr, sub_hash_table_configuration new_config, int retry_count, sub_hash_table_memory_pool** new_sub_hash_table_out);
static int _finalize_hash_bucket_resizing(hash_bucket* hash_bucket_ptr, sub_hash_table_configuration new_config, sub_hash_table_memory_pool* new_sub_hash_table_ptr);
#pragma endregion


#pragma region Public Function Definitions

int hash_bucket_resize_worker(void* input_arg)
{
    if (input_arg == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input

    hash_bucket* hash_bucket_ptr = (hash_bucket*)input_arg;

    sub_hash_table_configuration new_config = hash_bucket_ptr->sub_hash_table_config;
    new_config.bucket_size *= 2; // Example: double the bucket size during resizing

    
    sub_hash_table_memory_pool* new_sub_hash_table_ptr = NULL;
    _perform_hash_bucket_resizing(hash_bucket_ptr, new_config, 3, &new_sub_hash_table_ptr);

    //acquire resizing lock
    pthread_mutex_lock(&hash_bucket_ptr->resizing_lock);

    _finalize_hash_bucket_resizing(hash_bucket_ptr, new_config, new_sub_hash_table_ptr);

    pthread_mutex_unlock(&hash_bucket_ptr->resizing_lock);

    return SUCCESS;
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

    int attempt_result = 0;
    
    for (int attempt = 0; attempt < retry_count; ++attempt) {
        
        sub_hash_table_memory_pool* new_sub_hash_table_ptr = NULL;
        int create_result = create_new_sub_hash_table(new_config, true, &new_sub_hash_table_ptr);

        if(create_result == SUCCESS) {
            int fill_result = _fillup_new_sub_hash_table(hash_bucket_ptr, new_sub_hash_table_ptr);

            if(fill_result == SUCCESS) {
                *new_sub_hash_table_out = new_sub_hash_table_ptr;
                attempt_result = SUCCESS;
                break;
            }
            else
            {
                attempt_result = fill_result;
            }
        }
        else
        {
            attempt_result = create_result;
        }

        // Cleanup on failure before retrying
        if(new_sub_hash_table_ptr != NULL) {
            cleanup_sub_hash_table(new_sub_hash_table_ptr);
            new_sub_hash_table_ptr = NULL;
        }
    }

    return attempt_result; // Return the last error encountered
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

    unsigned int total_buckets = hash_bucket_ptr->snapshot_sub_hash_table_ptr->total_blocks;

    int result = SUCCESS;

    for(unsigned int index = 0; index < total_buckets; ++index) {
        
        sub_hash_bucket* current_sub_bucket = &hash_bucket_ptr->snapshot_sub_hash_table_ptr->sub_hash_buckets_ptr[index];

        if(current_sub_bucket == NULL || !current_sub_bucket->is_initialized) continue; // If sub-bucket is uninitialized, skip it

        linked_list_node* current_header = current_sub_bucket->linked_list_head;
        if(current_header == NULL) continue; // If linked list is empty, skip it
        
        result = _append_list_nodes_to_sub_hash_table(current_header, new_sub_hash_table_ptr);
        
        if(result != SUCCESS) break; // If any error occurs, the loop breaks and returns the error 
    }

    return result;
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

    int result = SUCCESS;
    
    // Apply pending operations: Apply all pending operations to the new sub-hash-table if it exists, otherwise to the snapshot sub-hash-table
    if(new_sub_hash_table_ptr != NULL)
    {
        result = _apply_pending_list_operations_to_sub_hash_table(hash_bucket_ptr, new_sub_hash_table_ptr);
    }
    else {
        result = _apply_pending_list_operations_to_sub_hash_table(hash_bucket_ptr, hash_bucket_ptr->snapshot_sub_hash_table_ptr);
    }

    if(result != SUCCESS)  return result; // Propagate error

    
    // Swap hash tables: Swap in the new sub-hash-table if it was created, otherwise retain the snapshot sub-hash-table
    if(new_sub_hash_table_ptr != NULL) {
        hash_bucket_ptr->sub_hash_table_ptr = new_sub_hash_table_ptr;
        hash_bucket_ptr->sub_hash_table_config = new_config;
        
        if(hash_bucket_ptr->snapshot_sub_hash_table_ptr != NULL) {
            cleanup_sub_hash_table(hash_bucket_ptr->snapshot_sub_hash_table_ptr);
            hash_bucket_ptr->snapshot_sub_hash_table_ptr = NULL;
        }
    }
    else {
        hash_bucket_ptr->sub_hash_table_ptr = hash_bucket_ptr->snapshot_sub_hash_table_ptr;
    }

    // Cleanup pending list and reset resizing state
    delete_all_linked_list_nodes(hash_bucket_ptr->pending_list_head);
    hash_bucket_ptr->pending_list_head = NULL;

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