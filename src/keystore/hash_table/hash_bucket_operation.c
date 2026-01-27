#include "hash_bucket_operation.h"
#include "sub_hash_table/sub_hash_table_operation.h"
#include "type_definitions/error_code_definitions.h"
#include "hash_table/hash_bucket_resizing_operation.h"
#include "utils/memory_manager.h"
#include "data_structures/linked_list_operation.h"
#include <stdio.h>
#include <stdlib.h>

#pragma region Prvate Function Definitions
int _resizing_lock_wrapper_for_hash_bucket_operation(hash_bucket_resizing_operation_t operation_type, hash_bucket* hash_bucket_ptr, const char *key, uint32_t key_hash, key_value_pair* kv_pair_out);
#pragma endregion


int initialise_hash_bucket(hash_bucket* hash_bucket_ptr, sub_hash_table_configuration sub_hash_table_config)
{
    if (hash_bucket_ptr == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input

    if(hash_bucket_ptr->is_initialized) return 0; // Already initialized

    sub_hash_table_memory_pool* sub_hash_table_ptr = NULL;
    int init_result = create_new_sub_hash_table(sub_hash_table_config, true, &sub_hash_table_ptr);
    if (init_result != 0) return  init_result; // Error handling: failed to initialize sub-hash-table

    init_result = pthread_mutex_init(&hash_bucket_ptr->resizing_lock, NULL);
    if (init_result != 0) {
        cleanup_sub_hash_table(sub_hash_table_ptr);
        return ERR_RESOURCE_INIT_FAILED; // Error handling: failed to initialize mutex
    }

    hash_bucket_ptr->sub_hash_table_ptr = sub_hash_table_ptr;
    hash_bucket_ptr->snapshot_sub_hash_table_ptr = NULL;
    hash_bucket_ptr->pending_list_head = NULL;
    hash_bucket_ptr->node_count = 0;
    hash_bucket_ptr->is_resizing = false;
    hash_bucket_ptr->is_initialized = true;
    hash_bucket_ptr->sub_hash_table_config = sub_hash_table_config;
    
    return 0;
}

int cleanup_hash_bucket(hash_bucket* hash_bucket_ptr)
{
    if (hash_bucket_ptr == NULL || !hash_bucket_ptr->is_initialized) return 0; // Nothing to clean up
    
    // Clean up current sub-hash-table
    if(hash_bucket_ptr->sub_hash_table_ptr != NULL) {
        cleanup_sub_hash_table(hash_bucket_ptr->sub_hash_table_ptr);
        free_memory(hash_bucket_ptr->sub_hash_table_ptr, false);
        hash_bucket_ptr->sub_hash_table_ptr = NULL;
    }

    // Clean up snapshot sub-hash-table
    if(hash_bucket_ptr->snapshot_sub_hash_table_ptr != NULL) {
        cleanup_sub_hash_table(hash_bucket_ptr->snapshot_sub_hash_table_ptr);
        free_memory(hash_bucket_ptr->snapshot_sub_hash_table_ptr, false);
        hash_bucket_ptr->snapshot_sub_hash_table_ptr = NULL;
    }

    // Clean up pending list
    if(hash_bucket_ptr->pending_list_head != NULL) {
        delete_all_linked_list_nodes(hash_bucket_ptr->pending_list_head);
        hash_bucket_ptr->pending_list_head = NULL;
    }

    pthread_mutex_destroy(&hash_bucket_ptr->resizing_lock);

    hash_bucket_ptr->node_count = 0;
    hash_bucket_ptr->is_resizing = false;
    hash_bucket_ptr->is_initialized = false;

    return 0;
}


int upsert_node_to_hash_bucket(hash_bucket* hash_bucket_ptr, uint32_t key_hash, key_value_pair* kv_pair)
{
    int result = 0;
    if (hash_bucket_ptr == NULL || kv_pair == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input
    
    if(!hash_bucket_ptr->is_initialized) return ERR_HASH_BUCKET_NOT_INITIALIZED; // Error handling: hash bucket not initialized

    if(!hash_bucket_ptr->is_resizing) {
        
        if(hash_bucket_ptr->sub_hash_table_ptr == NULL) return ERR_SUB_HASH_TABLE_NOT_INITIALIZED; // Error handling: sub-hash-table not initialized
        
        result = upsert_node_to_sub_hash_table(hash_bucket_ptr->sub_hash_table_ptr, key_hash, kv_pair);
    }
    else
    {
        result = _resizing_lock_wrapper_for_hash_bucket_operation(RESIZE_UPSERT_NODE, hash_bucket_ptr, NULL, key_hash, kv_pair);
    }

    if(result == 20)
    {   
        int re_result = _resizing_lock_wrapper_for_hash_bucket_operation(RESIZE_INITIALIZE, hash_bucket_ptr, NULL, 0, NULL);

        if(re_result != SUCCESS) {
            return re_result; // Propagate error
        }
    }
    
    return result;
}

int get_key_value_from_hash_bucket(hash_bucket* hash_bucket_ptr, const char* key, uint32_t key_hash, key_value_pair* kv_pair_out)
{
    if (hash_bucket_ptr == NULL || kv_pair_out == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input

    if(!hash_bucket_ptr->is_initialized) return ERR_HASH_BUCKET_NOT_INITIALIZED; // Error handling: hash bucket not initialized
    
    if(!hash_bucket_ptr->is_resizing) {
        if(hash_bucket_ptr->sub_hash_table_ptr == NULL) return ERR_SUB_HASH_TABLE_NOT_INITIALIZED; // Error handling: sub-hash-table not initialized
        return get_key_store_value_from_sub_hash_table(hash_bucket_ptr->sub_hash_table_ptr, key_hash, key, kv_pair_out);
    }
    else
    {
        return _resizing_lock_wrapper_for_hash_bucket_operation(RESIZE_GET_NODE, hash_bucket_ptr, key, key_hash, kv_pair_out);
    }
}

int delete_key_from_hash_bucket(hash_bucket* hash_bucket_ptr, const char* key, uint32_t key_hash)
{
    if (hash_bucket_ptr == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input

    if(!hash_bucket_ptr->is_initialized) return ERR_HASH_BUCKET_NOT_INITIALIZED; // Error handling: hash bucket not initialized

    if(!hash_bucket_ptr->is_resizing) {
        if(hash_bucket_ptr->sub_hash_table_ptr == NULL) return ERR_SUB_HASH_TABLE_NOT_INITIALIZED; // Error handling: sub-hash-table not initialized
        return delete_key_from_sub_hash_table(hash_bucket_ptr->sub_hash_table_ptr, key_hash, key);
    }
    else
    {
        return _resizing_lock_wrapper_for_hash_bucket_operation(RESIZE_DELETE_NODE, hash_bucket_ptr, key, key_hash, NULL);
    }
}

#pragma region Private Function Definitions

int _resizing_lock_wrapper_for_hash_bucket_operation(hash_bucket_resizing_operation_t operation_type, hash_bucket* hash_bucket_ptr, const char *key, uint32_t key_hash, key_value_pair* kv_pair_out)
{
    int lock_result = pthread_mutex_lock(&hash_bucket_ptr->resizing_lock);
    if (lock_result != 0) return ERR_MUTEX_LOCK_ACQUIRE_FAILED; // Error handling: failed to acquire lock
    int result = 0;
    switch(operation_type) {
        case RESIZE_INITIALIZE:
            result = initialize_hash_bucket_resizing(hash_bucket_ptr);
            break;
        case RESIZE_UPSERT_NODE:
            result = upsert_node_to_hash_bucket_during_resizing(hash_bucket_ptr, key_hash, kv_pair_out);
            break;
        case RESIZE_GET_NODE:
            result = get_key_value_from_hash_bucket_during_resizing(hash_bucket_ptr, key, key_hash, kv_pair_out);
            break;
        case RESIZE_DELETE_NODE:
            result = delete_key_from_hash_bucket_during_resizing(hash_bucket_ptr, key, key_hash);
            break;
        default:
            result = ERR_UNSUPPORTED_HASH_BUCKET_OP; // Invalid operation type
            break;
    }

    int unlock_result = pthread_mutex_unlock(&hash_bucket_ptr->resizing_lock);
    if (unlock_result != 0) return ERR_MUTEX_LOCK_RELEASE_FAILED; // Error handling: failed to release lock

    return result;
}

#pragma endregion