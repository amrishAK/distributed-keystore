#include "hash_bucket_resizing_operation.h"
#include "data_structures/data_node_operation.h"
#include "data_structures/linked_list_operation.h"
#include "sub_hash_table/sub_hash_bucket_operation.h"
#include "sub_hash_table/sub_hash_table_operation.h"
#include "utils/helper_functions.h"
#include <string.h>

#pragma region Resizing Helper FUnctions Declarations
static int _apply_pending_list_operations_to_sub_hash_table(hash_bucket* hash_bucket_ptr, sub_hash_table_memory_pool* target_sub_hash_table_ptr);
static int _append_list_nodes_to_sub_hash_table(linked_list_node* source_linked_list_head, sub_hash_table_memory_pool* sub_hash_table_ptr);
static int _fillup_new_sub_hash_table(hash_bucket* hash_bucket_ptr, sub_hash_table_memory_pool* new_sub_hash_table_ptr);
static int _perform_hash_bucket_resizing(hash_bucket* hash_bucket_ptr, sub_hash_table_configuration new_config, int retry_count, sub_hash_table_memory_pool** new_sub_hash_table_out);
static int _finalize_hash_bucket_resizing(hash_bucket* hash_bucket_ptr, sub_hash_table_configuration new_config, sub_hash_table_memory_pool* new_sub_hash_table_ptr);
#pragma endregion

#pragma region Private Function Definitions
int _find_node_in_hash_bucket_during_resizing(hash_bucket* hash_bucket_ptr, uint32_t key_hash, key_value_pair* key_value_pair_out);
int _add_node_to_pending_list(hash_bucket* hash_bucket_ptr, uint32_t key_hash, key_value_pair* kv_pair, bool is_delete_operation);
int _resize_hash_bucket(void* hash_bucket_ptr);
#pragma endregion


#pragma region Public Function Definitions

int initialize_hash_bucket_resizing(hash_bucket* hash_bucket_ptr)
{
    if (hash_bucket_ptr == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input

    hash_bucket_ptr->is_resizing = true;
    hash_bucket_ptr->snapshot_sub_hash_table_ptr = hash_bucket_ptr->sub_hash_table_ptr;
    hash_bucket_ptr->sub_hash_table_ptr = NULL; // New sub-hash-table will be assigned later

    int result = initialize_background_function(_resize_hash_bucket, (void*)hash_bucket_ptr);

    return result;
}


int upsert_node_to_hash_bucket_during_resizing(hash_bucket* hash_bucket_ptr, uint32_t key_hash, key_value_pair* kv_pair) {
    if (hash_bucket_ptr == NULL || kv_pair == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input

    //If the resizing finished while waiting for the lock, proceed with normal upsert
    if(!hash_bucket_ptr->is_resizing) {
        return upsert_node_to_sub_hash_table(hash_bucket_ptr->sub_hash_table_ptr, key_hash, kv_pair);
    }

    int result = 0;
    data_node* target_data_node = NULL;
    result = get_data_node_from_linked_list(hash_bucket_ptr->pending_list_head, kv_pair->key, key_hash, &target_data_node);
    
    if(result == SUCCESS) {
        // Node found, update it
        return edit_data_node_value(target_data_node, kv_pair);
    }
    else if(result == ERR_DATA_NODE_NOT_FOUND) {
        // Node not found, add to pending list
        return _add_node_to_pending_list(hash_bucket_ptr, key_hash, kv_pair, false);
    }
    else {
        return result; // Propagate other errors
    }
}

int get_key_value_from_hash_bucket_during_resizing(hash_bucket* hash_bucket_ptr, const char *key, uint32_t key_hash, key_value_pair* kv_pair_out) {
    if (hash_bucket_ptr == NULL || kv_pair_out == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input

    //If the resizing finished while waiting for the lock, proceed with normal upsert
    if(!hash_bucket_ptr->is_resizing) {
        return get_key_store_value_from_sub_hash_table(hash_bucket_ptr->sub_hash_table_ptr, key_hash, key, kv_pair_out);
    }

    return _find_node_in_hash_bucket_during_resizing(hash_bucket_ptr, key_hash, kv_pair_out);
}

int delete_key_from_hash_bucket_during_resizing(hash_bucket* hash_bucket_ptr, const char *key, uint32_t key_hash) {
    if (hash_bucket_ptr == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input

    //If the resizing finished while waiting for the lock, proceed with normal upsert
    if(!hash_bucket_ptr->is_resizing) {
        return delete_key_from_sub_hash_table(hash_bucket_ptr->sub_hash_table_ptr, key_hash, key);
    }

    int result = 0;
    data_node* target_data_node = NULL;
    result = get_data_node_from_linked_list(hash_bucket_ptr->pending_list_head, key, key_hash, &target_data_node);
    
    if(result == SUCCESS) {
        // Node found, update it
        return soft_delete_data_node(target_data_node);
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

#pragma endregion

#pragma region Private Function Definitions

int _find_node_in_hash_bucket_during_resizing(hash_bucket* hash_bucket_ptr, uint32_t key_hash, key_value_pair* key_value_pair_out)
{
    if (hash_bucket_ptr == NULL || key_value_pair_out == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input

    int result = 0;
    
    // First, check in the pending list
    data_node* target_data_node = NULL;
    result = get_data_node_from_linked_list(hash_bucket_ptr->pending_list_head, NULL, key_hash, &target_data_node);
    if (result == SUCCESS) {
        return read_data_node_value(target_data_node, key_value_pair_out);
    }

    // Next, check in the snapshot sub-hash-table
    if (hash_bucket_ptr->snapshot_sub_hash_table_ptr != NULL) {
        result = get_key_store_value_from_sub_hash_table(hash_bucket_ptr->snapshot_sub_hash_table_ptr, key_hash, NULL, key_value_pair_out);
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
    int result = create_new_data_node(key_hash, kv_pair, hash_bucket_ptr->config.is_concurrency_enabled, &new_data_node);
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

    return (result == SUCCESS) ? 10: result; // Return 10 to indicate node added to pending list successfully
}


int _resize_hash_bucket(void* input_arg)
{
    if (input_arg == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input

    hash_bucket* hash_bucket_ptr = (hash_bucket*)input_arg;
    
    sub_hash_table_configuration new_config = {
        .is_concurrency_enabled = hash_bucket_ptr->config.is_concurrency_enabled,
        .bucket_size = hash_bucket_ptr->config.bucket_size * 2, // Double the bucket size
        .max_linked_list_Chain_length = hash_bucket_ptr->config.max_linked_list_Chain_length
    };

    sub_hash_table_memory_pool* new_sub_hash_table_ptr = NULL;
    _perform_hash_bucket_resizing(hash_bucket_ptr, new_config, 3, &new_sub_hash_table_ptr);

    
    //acquire resizing lock
    pthread_mutex_lock(&hash_bucket_ptr->resizing_lock);

    _finalize_hash_bucket_resizing(hash_bucket_ptr, new_config, new_sub_hash_table_ptr);

    pthread_mutex_unlock(&hash_bucket_ptr->resizing_lock);

    // Placeholder for resizing logic
    return SUCCESS;
}

#pragma endregion

#pragma region Resizing Helper Function Definitions

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
        }

        continue;
    }

    new_sub_hash_table_out = NULL;
    return result; // Return the last error encountered
}

int _fillup_new_sub_hash_table(hash_bucket* hash_bucket_ptr, sub_hash_table_memory_pool* new_sub_hash_table_ptr)
{
    if (hash_bucket_ptr == NULL || new_sub_hash_table_ptr == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input

    for(unsigned int i = 0; i < hash_bucket_ptr->snapshot_sub_hash_table_ptr->block_size; ++i) {
        sub_hash_bucket* current_sub_bucket = &hash_bucket_ptr->snapshot_sub_hash_table_ptr->sub_hash_buckets_ptr[i];
        linked_list_node* current_header = current_sub_bucket->linked_list_head;
        
        int result = _append_list_nodes_to_sub_hash_table(current_header, new_sub_hash_table_ptr);
        if(result != SUCCESS) {
            return result; // Propagate error
        }
    }

    return SUCCESS;
}

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

    if(result != SUCCESS) {
        return result; // Propagate error
    }

    //Swap in the new sub-hash-table
    if(new_sub_hash_table_ptr != NULL) {
        hash_bucket_ptr->sub_hash_table_ptr = new_sub_hash_table_ptr;
        hash_bucket_ptr->config = new_config;
        if(hash_bucket_ptr->snapshot_sub_hash_table_ptr != NULL) {
            cleanup_sub_hash_table(hash_bucket_ptr->snapshot_sub_hash_table_ptr);
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

int _append_list_nodes_to_sub_hash_table(linked_list_node* source_linked_list_head, sub_hash_table_memory_pool* sub_hash_table_ptr)
{
    if (sub_hash_table_ptr == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input

    linked_list_node* current_node = source_linked_list_head;
    
    while (current_node != NULL) {

        if(current_node->data_node_ptr-> is_deleted) {
            current_node = current_node->next_node_ptr;
            continue;
        }
        
        unsigned int index = current_node->key_hash % sub_hash_table_ptr->block_size;
        
        if(index >= sub_hash_table_ptr->block_size) {
            return ERR_INVALID_SUB_BUCKET_INDEX; // Error handling: invalid sub-bucket index
        }

        sub_hash_bucket* target_sub_bucket = &sub_hash_table_ptr->sub_hash_buckets_ptr[index];

        sub_hash_bucket_operation_args args = {target_sub_bucket, current_node->data_node_ptr->key, current_node->key_hash};
        key_value_pair new_value = {
            .key = current_node->data_node_ptr->key,
            .value = current_node->data_node_ptr->data,
            .value_size = current_node->data_node_ptr->data_size
        };

        int add_result = add_node_to_sub_hash_bucket(args, &new_value);
        if (add_result != SUCCESS) {
            return add_result; // Propagate error
        }
  
        current_node = current_node->next_node_ptr;
    }

    return SUCCESS;
}

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
            if (upsert_result != SUCCESS) {
                return upsert_result; // Propagate error
            }
        }

        current_node = current_node->next_node_ptr;
    }

    return SUCCESS;
}

#pragma endregion