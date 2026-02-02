#include "hash_bucket_resizing_operation.h"
#include "data_structures/data_node_operation.h"
#include "data_structures/linked_list_operation.h"
#include "sub_hash_table/sub_hash_bucket_operation.h"
#include "sub_hash_table/sub_hash_table_operation.h"
#include "resize_operation.h"
#include "utils/helper_functions.h"
#include <string.h>


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

    return initialize_background_function(hash_bucket_resize_worker, (void*)hash_bucket_ptr);
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
