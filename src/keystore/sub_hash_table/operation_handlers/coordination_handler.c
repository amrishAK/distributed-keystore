#include "coordination_handler.h"
#include "data_structures/linked_list_operation.h"
#include "data_structures/data_node_operation.h"
#include "data_structures/bloom_filter_operation.h"
#include "type_definitions/error_code_definitions.h"
#include "type_definitions/sucess_code_definitions.h"
#include "utils/memory_manager.h"


#pragma region private Concurrency Lock Wrapper Declarations
static int _lock_wrapper_for_linked_list_node_operation(const linked_list_node_operation_t operation_type, const sub_hash_bucket_operation_args args, linked_list_node* new_node, data_node** data_node_out);
#pragma endregion

#pragma region Private Function Declarations
static int _cleanup_deleted_nodes_and_refill_bloom_filter(sub_hash_bucket* sub_hash_bucket_ptr);
static int _insert_node_and_add_to_bloom_filter(sub_hash_bucket* sub_hash_bucket_ptr, composite_key_hash key_hash, linked_list_node* new_node);
static int _find_node(sub_hash_bucket* sub_hash_bucket_ptr, const char* key, composite_key_hash key_hash, data_node** data_node_out);
#pragma endregion


#pragma region Public Function Definitions

int find_data_node(const sub_hash_bucket_operation_args args, data_node** data_node_out)
{
    return (args.sub_hash_bucket_ptr->is_concurrency_enabled) ? _lock_wrapper_for_linked_list_node_operation(GET_LL_NODE, args, NULL, data_node_out) : _find_node(args.sub_hash_bucket_ptr, args.key, args.key_hash, data_node_out);
}


int add_data_node(const sub_hash_bucket_operation_args args, data_node* new_node)
{
    linked_list_node* new_list_node = NULL;
    int result = create_new_linked_list_node(args.key_hash, new_node, &new_list_node);
    
    if (result != SUCCESS) {
        delete_data_node(new_node);
        return ERR_MEMORY_ALLOCATION_FAILED; // Error handling: memory allocation failure
    }

    int insert_result = (args.sub_hash_bucket_ptr->is_concurrency_enabled) ? _lock_wrapper_for_linked_list_node_operation(INSERT_LL_NODE, args, new_list_node, NULL) : _insert_node_and_add_to_bloom_filter(args.sub_hash_bucket_ptr, args.key_hash, new_list_node);
    
    if (insert_result != SUCCESS) {
        delete_data_node(new_node);
        free_memory(new_list_node, false);
        return insert_result; // Error handling: failed to insert linked list node
    }

    return SUCCESS; // Return success if insertion was successful
}

int delete_all_data_nodes(const sub_hash_bucket_operation_args args)
{
    int result = (args.sub_hash_bucket_ptr->is_concurrency_enabled) ? _lock_wrapper_for_linked_list_node_operation(DELETE_ALL_LL_NODES, args, NULL, NULL) : delete_all_linked_list_nodes(&args.sub_hash_bucket_ptr->linked_list_head);
   
   if (args.sub_hash_bucket_ptr->is_bloom_filter_enabled)
    {
        reset_bloom_filter(args.sub_hash_bucket_ptr->bloom_filter_ptr);
    }
    
    return result;
}

int cleanup_deleted_data_nodes(const sub_hash_bucket_operation_args args, unsigned int* deleted_count_out)
{
    if(deleted_count_out == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid output pointer

    int result = (args.sub_hash_bucket_ptr->is_concurrency_enabled) ? _lock_wrapper_for_linked_list_node_operation(CLEANUP_DELETED_LL_NODES, args, NULL, NULL) : _cleanup_deleted_nodes_and_refill_bloom_filter(args.sub_hash_bucket_ptr);    
    
    if (result < SUCCESS) return result; // Error handling: failed to cleanup deleted linked list nodes

    *deleted_count_out = (result > 0) ? (unsigned int)result : 0; // Set deleted count to 0 if no nodes were cleaned up or if an error occurred

    return result;
}


#pragma endregion


#pragma region Private Function Definitions

static int _cleanup_deleted_nodes_and_refill_bloom_filter(sub_hash_bucket* sub_hash_bucket_ptr)
{
    int result = cleanup_deleted_linked_list_nodes(&sub_hash_bucket_ptr->linked_list_head);

    // Skip bloom filter refill if no nodes were cleaned up or if bloom filter is not enabled
    if(result <= 0 || !sub_hash_bucket_ptr->is_bloom_filter_enabled) {
        return result; // Error handling: failed to cleanup deleted linked list nodes or no nodes were cleaned up
    }

    // reset the bloom filter
    if(sub_hash_bucket_ptr->is_bloom_filter_enabled)
    {
        reset_bloom_filter(sub_hash_bucket_ptr->bloom_filter_ptr);
    }

    // Re-add all active nodes to the bloom filter
    linked_list_node* current_node = sub_hash_bucket_ptr->linked_list_head;
    while (current_node != NULL) {
        if (!current_node->data_node_ptr->is_deleted && sub_hash_bucket_ptr->is_bloom_filter_enabled) {
            add_key_to_bloom_filter(current_node->data_node_ptr->key_hash, sub_hash_bucket_ptr->bloom_filter_ptr);
        }
        current_node = current_node->next_node_ptr;
    }

    return result; // Return the count of deleted nodes that were cleaned up
}

static int _insert_node_and_add_to_bloom_filter(sub_hash_bucket* sub_hash_bucket_ptr, composite_key_hash key_hash, linked_list_node* new_node)
{
    int result = insert_linked_list_node(&sub_hash_bucket_ptr->linked_list_head, new_node);

    if(result != SUCCESS) return result; // Error handling: failed to insert linked list node

    if(sub_hash_bucket_ptr->is_bloom_filter_enabled) {
        
        int bloom_result = add_key_to_bloom_filter(key_hash, sub_hash_bucket_ptr->bloom_filter_ptr);
        
        if(bloom_result != SUCCESS) {
            // Keep the inserted node and degrade to linked-list lookup only.
            cleanup_bloom_filter(sub_hash_bucket_ptr->bloom_filter_ptr);
            sub_hash_bucket_ptr->bloom_filter_ptr = NULL;
            sub_hash_bucket_ptr->is_bloom_filter_enabled = false;
        }
    }

    return result;
}

static int _find_node(sub_hash_bucket* sub_hash_bucket_ptr, const char* key, composite_key_hash key_hash, data_node** data_node_out)
{
    if(!sub_hash_bucket_ptr->is_bloom_filter_enabled || sub_hash_bucket_ptr->bloom_filter_ptr == NULL) {
        return get_data_node_from_linked_list(sub_hash_bucket_ptr->linked_list_head, key, key_hash, false, data_node_out);
    }

    int bloom_result = check_key_in_bloom_filter(key_hash, sub_hash_bucket_ptr->bloom_filter_ptr);

    if (bloom_result == BLOOM_FILTER_CHECK_KEY_NOT_EXIST) return ERR_DATA_NODE_NOT_FOUND; // Key definitely does not exist

    // On bloom errors, fall back to exact lookup to preserve correctness.
    return get_data_node_from_linked_list(sub_hash_bucket_ptr->linked_list_head, key, key_hash, false, data_node_out);
}

#pragma endregion


#pragma region Private Concurrency Lock Wrapper Definitions

/** 
 * @fn _lock_wrapper_for_linked_list_node_operation
 * @brief Concurrency lock wrapper for linked list node operations.
 * Acquires the appropriate lock (read or write) on the sub-hash-bucket,
 * performs the specified linked list operation, and releases the lock.
 * @param operation_type The type of linked list operation to perform.
 * @param args Structure containing sub-hash-bucket pointer, key, and key hash.
 * @param new_node Pointer to a new linked list node (for insert operations).
 * @param data_node_out Pointer to receive a data node (for get operations).
 * @param deleted_count_out Pointer to receive count of deleted nodes (for cleanup operations).
 * @param operation_result_out Pointer to receive operation result (for get operations).
 * @return int Returns the result of the linked list operation.
 */
int _lock_wrapper_for_linked_list_node_operation(const linked_list_node_operation_t operation_type, const sub_hash_bucket_operation_args args, linked_list_node* new_node, data_node** data_node_out)
{
    int lock_result = 0;

    // Acquire appropriate lock based on operation type
    if (operation_type == GET_LL_NODE) {
        lock_result = pthread_rwlock_rdlock(&args.sub_hash_bucket_ptr->sub_hash_bucket_lock);
    } else {
        lock_result = pthread_rwlock_wrlock(&args.sub_hash_bucket_ptr->sub_hash_bucket_lock);
    }

    if(lock_result != 0) return ERR_RW_LOCK_ACQUIRE_FAILED; // Error handling: failed to acquire lock

    int result = 0;
    // Perform the linked list node operation
    switch (operation_type) {
        case INSERT_LL_NODE:
            result = _insert_node_and_add_to_bloom_filter(args.sub_hash_bucket_ptr, args.key_hash, new_node);
            break;
        case GET_LL_NODE:
            result = _find_node(args.sub_hash_bucket_ptr, args.key, args.key_hash, data_node_out);
            break;
        case DELETE_ALL_LL_NODES:
            result = delete_all_linked_list_nodes(&args.sub_hash_bucket_ptr->linked_list_head);
            break;
        case CLEANUP_DELETED_LL_NODES:
            result = _cleanup_deleted_nodes_and_refill_bloom_filter(args.sub_hash_bucket_ptr);
            break;
        default:
            result = ERR_UNSUPPORTED_HASH_BUCKET_OP; // Error handling: unsupported operation type
            break;
    }

    // Release the lock
    lock_result = pthread_rwlock_unlock(&args.sub_hash_bucket_ptr->sub_hash_bucket_lock);

    // Handle unlock error: if the operation failed, return that result; otherwise, return unlock error
    if(lock_result != SUCCESS) {
        if(result != SUCCESS) return result; // Return the original operation result if it failed
        return ERR_RW_LOCK_RELEASE_FAILED; // Error handling: failed to release lock
    }

    return result;
}
#pragma endregion
