#include "resize_operation.h"
#include "sub_hash_table/sub_hash_table_operation.h"
#include "data_structures/linked_list_operation.h"
#include "buffer_operation.h"
#include "utils/memory_manager.h"
#include "type_definitions/background_task_manager_type_definitons.h"

#include <stdio.h>

#pragma region Resizing Helper Functions Declarations
static int _append_list_nodes_to_sub_hash_table(linked_list_node* source_linked_list_head, sub_hash_table_memory_pool* sub_hash_table_ptr);
static int _fillup_new_sub_hash_table(hash_bucket* hash_bucket_ptr, sub_hash_table_memory_pool* new_sub_hash_table_ptr);
static int _perform_hash_bucket_resizing(hash_bucket* hash_bucket_ptr, sub_hash_table_configuration new_config, int retry_count);
static int _finalize_hash_bucket_resizing(hash_bucket* hash_bucket_ptr, sub_hash_table_configuration new_config);
#pragma endregion


#pragma region Public Function Definitions

int hash_bucket_resize_worker(void* input_arg)
{
    if (input_arg == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input

    background_task_args_t* args = (background_task_args_t*)input_arg;
    hash_bucket* hash_bucket_ptr = (hash_bucket*)args->task_input_args;

    

    sub_hash_table_configuration new_config = {0};
    new_config.bucket_size = hash_bucket_ptr->sub_hash_table_config.bucket_size * 2; // Example resizing strategy: double the bucket size
    new_config.is_concurrency_enabled = hash_bucket_ptr->sub_hash_table_config.is_concurrency_enabled;
    new_config.max_linked_list_chain_length = hash_bucket_ptr->sub_hash_table_config.max_linked_list_chain_length;
    
    _perform_hash_bucket_resizing(hash_bucket_ptr, new_config, 3);

    //acquire resizing lock
    pthread_mutex_lock(&hash_bucket_ptr->resizing_lock);

    _finalize_hash_bucket_resizing(hash_bucket_ptr, new_config);

    
    delete_resizing_buffer(hash_bucket_ptr->resizing_buffer_ptr);
    free_memory(hash_bucket_ptr->resizing_buffer_ptr, false);
    hash_bucket_ptr->resizing_buffer_ptr = NULL;
    hash_bucket_ptr->is_resizing = false;
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
int _perform_hash_bucket_resizing(hash_bucket* hash_bucket_ptr, sub_hash_table_configuration new_config, int retry_count)
{
    if (hash_bucket_ptr == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input
    if (retry_count <= 0) return ERR_MAX_RETRY_EXCEEDED; // Error handling: max retries exceeded

    if(hash_bucket_ptr->resizing_buffer_ptr == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid

    int attempt_result = 0;
    
    for (int attempt = 0; attempt < retry_count; ++attempt) {
        
        sub_hash_table_memory_pool* new_sub_hash_table_ptr = NULL;
        int create_result = create_new_sub_hash_table(new_config, true, &new_sub_hash_table_ptr);

        if(new_sub_hash_table_ptr != NULL) {
            hash_bucket_ptr->resizing_buffer_ptr->new_sub_hash_table_ptr = new_sub_hash_table_ptr;
            
        }
        else
        {
            attempt_result = ERR_FAILURE;
        }

        if(create_result == SUCCESS) {
            uint32_t task_uuid;
            int fill_result;
            int result = initialize_chase_worker(hash_bucket_ptr->resizing_buffer_ptr, &task_uuid);
            
            
            if(result == SUCCESS) {
                fill_result = _fillup_new_sub_hash_table(hash_bucket_ptr, new_sub_hash_table_ptr);
                
            }
            
            result = wait_for_chase_worker_to_finish(hash_bucket_ptr->resizing_buffer_ptr, task_uuid);
            

            if (result != SUCCESS)
            {
                fill_result = result; // If waiting for the chase worker failed, treat it as a failure for filling the new sub-hash-table
            }
            

            // If filling is successful, set the new sub-hash-table in the resizing buffer to be swapped in later.
            if(fill_result == SUCCESS) {
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
        if(hash_bucket_ptr->resizing_buffer_ptr->new_sub_hash_table_ptr != NULL) {
            cleanup_sub_hash_table(hash_bucket_ptr->resizing_buffer_ptr->new_sub_hash_table_ptr);
            free_memory(hash_bucket_ptr->resizing_buffer_ptr->new_sub_hash_table_ptr, false);
            hash_bucket_ptr->resizing_buffer_ptr->new_sub_hash_table_ptr = NULL;
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
int _finalize_hash_bucket_resizing(hash_bucket* hash_bucket_ptr, sub_hash_table_configuration new_config)
{
    if (hash_bucket_ptr == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input

    int result = SUCCESS;
    
    result = commit_resizing_buffer_operations_to_sub_hash_table(hash_bucket_ptr);
    if(result != SUCCESS) return result;
    
    // Swap hash tables: Swap in the new sub-hash-table if it was created, otherwise retain the snapshot sub-hash-table
    if(hash_bucket_ptr->resizing_buffer_ptr->new_sub_hash_table_ptr != NULL) {
        
        hash_bucket_ptr->sub_hash_table_ptr = hash_bucket_ptr->resizing_buffer_ptr->new_sub_hash_table_ptr;
        hash_bucket_ptr->sub_hash_table_config = new_config;
        hash_bucket_ptr->resizing_buffer_ptr->new_sub_hash_table_ptr = NULL; // Clear the pointer in the resizing buffer since it's now active

        if(hash_bucket_ptr->snapshot_sub_hash_table_ptr != NULL) {
            cleanup_sub_hash_table(hash_bucket_ptr->snapshot_sub_hash_table_ptr);
            free_memory(hash_bucket_ptr->snapshot_sub_hash_table_ptr, false);
            hash_bucket_ptr->snapshot_sub_hash_table_ptr = NULL;
        }
    }
    else {
        hash_bucket_ptr->sub_hash_table_ptr = hash_bucket_ptr->snapshot_sub_hash_table_ptr;
        hash_bucket_ptr->snapshot_sub_hash_table_ptr = NULL; // Clear the snapshot pointer since it's now active
    }

    // Cleanup pending list and reset resizing state

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

        int result = upsert_node_to_sub_hash_table(sub_hash_table_ptr, current_node->data_node_ptr->key_hash, &kv_pair);
        if (result < SUCCESS) return result; // If any error occurs during upsert, return the error code

        current_node = current_node->next_node_ptr;
    }

    return SUCCESS;
}

#pragma endregion