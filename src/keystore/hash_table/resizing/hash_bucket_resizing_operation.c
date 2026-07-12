#include "hash_bucket_resizing_operation.h"
#include "data_structures/data_node_operation.h"
#include "data_structures/linked_list_operation.h"
#include "sub_hash_table/sub_hash_bucket_operation.h"
#include "sub_hash_table/sub_hash_table_operation.h"
#include "resize_operation.h"
#include "buffer_operation.h"
#include "utils/background_task_manager.h"
#include <string.h>
#include "utils/memory_manager.h"

#include <stdio.h>


#pragma region Private Function Definitions
int _find_node_in_hash_bucket_during_resizing(hash_bucket* hash_bucket_ptr, const char* key, composite_key_hash key_hash, key_value_pair* key_value_pair_out);
int _delete_node_while_resizing(hash_bucket* hash_bucket_ptr, const char *key, composite_key_hash key_hash);
int _update_node_while_resizing(hash_bucket* hash_bucket_ptr, composite_key_hash key_hash, key_value_pair* kv_pair);
#pragma endregion


#pragma region Public Function Definitions

int initialize_hash_bucket_resizing(hash_bucket* hash_bucket_ptr)
{
    if (hash_bucket_ptr == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input

    hash_bucket_ptr->snapshot_sub_hash_table_ptr = NULL;
    hash_bucket_ptr->is_resizing = true;
    hash_bucket_ptr->snapshot_sub_hash_table_ptr = hash_bucket_ptr->sub_hash_table_ptr;
    int result = initialize_resizing_buffer(hash_bucket_ptr);
    if (result != SUCCESS) {
        hash_bucket_ptr->is_resizing = false;
        hash_bucket_ptr->snapshot_sub_hash_table_ptr = NULL;
        return result;
    }

    result = initialize_background_function(hash_bucket_resize_worker, (void*)hash_bucket_ptr, true, NULL);
    if (result != SUCCESS) {
        if (hash_bucket_ptr->resizing_buffer_ptr != NULL) {
            delete_resizing_buffer(hash_bucket_ptr->resizing_buffer_ptr);
            free_memory(hash_bucket_ptr->resizing_buffer_ptr, false);
            hash_bucket_ptr->resizing_buffer_ptr = NULL;
        }
        hash_bucket_ptr->is_resizing = false;
        hash_bucket_ptr->snapshot_sub_hash_table_ptr = NULL;
    }

    return result;
}

int upsert_node_to_hash_bucket_during_resizing(hash_bucket* hash_bucket_ptr, composite_key_hash key_hash, key_value_pair* kv_pair) {
    if (hash_bucket_ptr == NULL || kv_pair == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input

    

    //If the resizing finished while waiting for the lock, proceed with normal upsert
    if(!hash_bucket_ptr->is_resizing) {
        return upsert_node_to_sub_hash_table(hash_bucket_ptr->sub_hash_table_ptr, key_hash, kv_pair);
    }

    return _update_node_while_resizing(hash_bucket_ptr, key_hash, kv_pair);
}

int get_key_value_from_hash_bucket_during_resizing(hash_bucket* hash_bucket_ptr, const char *key, composite_key_hash key_hash, key_value_pair* kv_pair_out) {
    if (hash_bucket_ptr == NULL || kv_pair_out == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input

    //If the resizing finished while waiting for the lock, proceed with normal upsert
    if(!hash_bucket_ptr->is_resizing) {
        int result = get_key_store_value_from_sub_hash_table(hash_bucket_ptr->sub_hash_table_ptr, key_hash, key, kv_pair_out);
        
        return result;
    }

    

    return _find_node_in_hash_bucket_during_resizing(hash_bucket_ptr, key, key_hash, kv_pair_out);
}

int delete_key_from_hash_bucket_during_resizing(hash_bucket* hash_bucket_ptr, const char *key, composite_key_hash key_hash) {
    if (hash_bucket_ptr == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input

    //If the resizing finished while waiting for the lock, proceed with normal upsert
    if(!hash_bucket_ptr->is_resizing) {
        return delete_key_from_sub_hash_table(hash_bucket_ptr->sub_hash_table_ptr, key_hash, key);
    }

    return _delete_node_while_resizing(hash_bucket_ptr, key, key_hash);
}

int check_resize_status(hash_bucket* hash_bucket_ptr) {
    if (hash_bucket_ptr == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input

    bool is_resizing = hash_bucket_ptr->is_resizing;

    if(is_resizing) {
        return 21; // Indicate that resizing is still in progress
    }
    
    return SUCCESS; // Resizing has completed
}

#pragma endregion

#pragma region Private Function Definitions

int _find_node_in_hash_bucket_during_resizing(hash_bucket* hash_bucket_ptr, const char* key, composite_key_hash key_hash, key_value_pair* key_value_pair_out)
{
    if (hash_bucket_ptr == NULL || key_value_pair_out == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input

    int result = ERR_DATA_NODE_NOT_FOUND;
    
    result = is_node_in_sub_hash_table(hash_bucket_ptr->snapshot_sub_hash_table_ptr, key_hash, key);

    if(result == SUCCESS)
    {
        result = get_node_from_resizing_buffer(hash_bucket_ptr, key_hash, key, true, key_value_pair_out);      

        if(result == ERR_DATA_NODE_NOT_FOUND) {
            // Node not found in current operation buffer, get from snapshot sub hash table
            result = get_key_store_value_from_sub_hash_table(hash_bucket_ptr->snapshot_sub_hash_table_ptr, key_hash, key, key_value_pair_out);
            return result;
        }
    }
    else{
        result = get_node_from_resizing_buffer(hash_bucket_ptr, key_hash, key, false, key_value_pair_out);
        
    }

    return result; // Node not found
}


int _delete_node_while_resizing(hash_bucket* hash_bucket_ptr, const char *key, composite_key_hash key_hash)
{
    int result = 0;
    
    result = is_node_in_sub_hash_table(hash_bucket_ptr->snapshot_sub_hash_table_ptr, key_hash, key);

    if(result == SUCCESS) {
        // Node found, add to delete operation list in buffer
        result = insert_delete_operation_to_resizing_buffer(hash_bucket_ptr, key_hash, key);
    }
    else if(result == ERR_DATA_NODE_NOT_FOUND) {
        // Node not found, add to current operation list in buffer
        unsigned char* dummy_value = (unsigned char*)"dummy_value"; // Use the key as the dummy key for deletion
        key_value_pair dummy_kv_pair = { .key = (char*)key, .value = dummy_value, .value_size = strlen((char*)dummy_value) };
        result = insert_node_to_new_operation_buffer(hash_bucket_ptr, key_hash, &dummy_kv_pair, true);
    }

    return result;
}

int _update_node_while_resizing(hash_bucket* hash_bucket_ptr, composite_key_hash key_hash, key_value_pair* kv_pair)
{
    int result = 0;
    
    result = is_node_in_sub_hash_table(hash_bucket_ptr->snapshot_sub_hash_table_ptr, key_hash, kv_pair->key);

    if(result == SUCCESS) {
        // Node found, add to update operation list in buffer
        result = insert_update_operation_to_resizing_buffer(hash_bucket_ptr, key_hash, kv_pair);
    }
    else if(result == ERR_DATA_NODE_NOT_FOUND) {
        // Node not found, add to current operation list in buffer
        result = insert_node_to_new_operation_buffer(hash_bucket_ptr, key_hash, kv_pair, false);
    }

    return result;
}

#pragma endregion
