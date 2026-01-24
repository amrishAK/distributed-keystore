#include "sub_hash_bucket_operation.h"
#include "data_structures/linked_list_operation.h"
#include "data_structures/data_node_operation.h"
#include "utils/memory_manager.h"

#pragma region private Concurrency Lock Wrapper Declarations
static int _lock_wrapper_for_linked_list_node_operation(linked_list_node_operation_t operation_type, sub_hash_bucket_operation_args args, linked_list_node* new_node, data_node** data_node_out);
static int _lock_wrapper_for_data_node_operation(data_node_operation_t operation_type, data_node* data_node_ptr, key_value_pair* value);
static int _check_for_resize_condition(sub_hash_bucket* sub_hash_bucket_ptr);
#pragma endregion

#pragma region Public Function Definitions

int initialise_sub_hash_bucket(sub_hash_bucket *sub_hash_bucket_ptr, bool is_concurrency_enabled, unsigned int max_linked_list_chain_length)
{
    if(sub_hash_bucket_ptr == NULL) return ERR_INVALID_ARGUMENT;

    if(is_concurrency_enabled) {
        if(pthread_rwlock_init(&sub_hash_bucket_ptr->sub_hash_bucket_lock, NULL) != 0) {
            return ERR_RESOURCE_INIT_FAILED; // Error handling: failed to initialize rw lock
        }
    }

    sub_hash_bucket_ptr->linked_list_head = NULL;
    sub_hash_bucket_ptr->active_node_count = 0;
    sub_hash_bucket_ptr->total_node_count = 0;
    sub_hash_bucket_ptr->is_initialized = true;
    sub_hash_bucket_ptr->is_concurrency_enabled = is_concurrency_enabled;
    sub_hash_bucket_ptr->max_linked_list_chain_length = max_linked_list_chain_length;

    return 0;
}

int cleanup_sub_hash_bucket(sub_hash_bucket* sub_hash_bucket_ptr)
{
    if(sub_hash_bucket_ptr == NULL) return ERR_INVALID_ARGUMENT;
    if(!sub_hash_bucket_ptr->is_initialized) return 0; // Nothing to clean up

    int result = 0;
    sub_hash_bucket_operation_args args = {
        .sub_hash_bucket_ptr = sub_hash_bucket_ptr,
        .key = NULL,
        .key_hash = 0
    };

    result = (sub_hash_bucket_ptr->is_concurrency_enabled) ? _lock_wrapper_for_linked_list_node_operation(DELETE_ALL_LL_NODES, args, NULL, NULL) : delete_all_linked_list_nodes(sub_hash_bucket_ptr->linked_list_head);

    if(sub_hash_bucket_ptr->is_concurrency_enabled) {
        result = pthread_rwlock_destroy(&sub_hash_bucket_ptr->sub_hash_bucket_lock);
    }

    sub_hash_bucket_ptr->linked_list_head = NULL;
    sub_hash_bucket_ptr->is_initialized = false;
    sub_hash_bucket_ptr->active_node_count = 0;
    sub_hash_bucket_ptr->is_concurrency_enabled = false;
    return result;
}

int update_node_in_sub_hash_bucket(sub_hash_bucket_operation_args args, key_value_pair* new_value)
{   
    if(new_value == NULL || args.sub_hash_bucket_ptr == NULL || args.key == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input

    int result = 0;
    data_node* target_data_node = NULL;
    result = (args.sub_hash_bucket_ptr->is_concurrency_enabled) ? _lock_wrapper_for_linked_list_node_operation(GET_LL_NODE, args, NULL, &target_data_node) : get_data_node_from_linked_list(args.sub_hash_bucket_ptr->linked_list_head, args.key, args.key_hash, &target_data_node);

    if (result != SUCCESS) return result; // Error handling: node not found

    result = (target_data_node->is_concurrency_enabled) ? _lock_wrapper_for_data_node_operation(UPDATE_NODE, target_data_node, new_value) : edit_data_node_value(target_data_node, new_value);

    return result;
}

int add_node_to_sub_hash_bucket(sub_hash_bucket_operation_args args, key_value_pair* new_value)
{
    if(new_value == NULL || args.sub_hash_bucket_ptr == NULL || args.key == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input


    data_node* new_data_node = NULL;
    int result = create_new_data_node(args.key_hash, new_value, args.sub_hash_bucket_ptr->is_concurrency_enabled, &new_data_node);
    if (result != SUCCESS) return result; // Error handling: failed to create new data node

    linked_list_node* new_list_node = NULL;
    result = create_new_linked_list_node(args.key_hash, new_data_node, &new_list_node);
    if (result != SUCCESS) {
        delete_data_node(new_data_node);
        return ERR_MEMORY_ALLOCATION_FAILED; // Error handling: memory allocation failure
    }

    int insert_result = (args.sub_hash_bucket_ptr->is_concurrency_enabled) ? _lock_wrapper_for_linked_list_node_operation(INSERT_LL_NODE, args, new_list_node, NULL) : insert_linked_list_node(&args.sub_hash_bucket_ptr->linked_list_head, new_list_node);
    
    if (insert_result != SUCCESS) {
        delete_data_node(new_data_node);
        free_memory(new_list_node, false);
        return insert_result; // Error handling: failed to insert linked list node
    }

    args.sub_hash_bucket_ptr->active_node_count++;
    args.sub_hash_bucket_ptr->total_node_count++;

    return _check_for_resize_condition(args.sub_hash_bucket_ptr);
}

int get_key_store_value_from_sub_hash_bucket(sub_hash_bucket_operation_args args, key_value_pair* value_out)
{
    if(args.sub_hash_bucket_ptr == NULL || args.key == NULL || value_out == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input

    data_node* target_data_node = NULL;
    int result = (args.sub_hash_bucket_ptr->is_concurrency_enabled) ? _lock_wrapper_for_linked_list_node_operation(GET_LL_NODE, args, NULL, &target_data_node) : get_data_node_from_linked_list(args.sub_hash_bucket_ptr->linked_list_head, args.key, args.key_hash, &target_data_node);

    if (result != SUCCESS) return result; // Error handling: node not found

    result = (target_data_node->is_concurrency_enabled) ? _lock_wrapper_for_data_node_operation(READ_NODE, target_data_node, value_out) : read_data_node_value(target_data_node, value_out);

    return result;
}

int delete_key_from_sub_hash_bucket(sub_hash_bucket_operation_args args)
{
    if(args.sub_hash_bucket_ptr == NULL || args.key == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input

    data_node* target_data_node = NULL;
    int result = (args.sub_hash_bucket_ptr->is_concurrency_enabled) ? _lock_wrapper_for_linked_list_node_operation(GET_LL_NODE, args, NULL, &target_data_node) : get_data_node_from_linked_list(args.sub_hash_bucket_ptr->linked_list_head, args.key, args.key_hash, &target_data_node);

    if (result != SUCCESS) return result; // Error handling: node not found

    result = (target_data_node->is_concurrency_enabled) ? _lock_wrapper_for_data_node_operation(SOFT_DELETE_NODE, target_data_node, NULL) : soft_delete_data_node(target_data_node);

    if (result == SUCCESS) {
        args.sub_hash_bucket_ptr->active_node_count--;
    }

    return result;
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
int _lock_wrapper_for_linked_list_node_operation(linked_list_node_operation_t operation_type, sub_hash_bucket_operation_args args, linked_list_node* new_node, data_node** data_node_out)
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
            result = insert_linked_list_node(&args.sub_hash_bucket_ptr->linked_list_head, new_node);
            break;
        case GET_LL_NODE:
            result = get_data_node_from_linked_list(args.sub_hash_bucket_ptr->linked_list_head, args.key, args.key_hash, data_node_out);
            break;
        case DELETE_ALL_LL_NODES:
            result = delete_all_linked_list_nodes(args.sub_hash_bucket_ptr->linked_list_head);
            break;
        case CLEANUP_DELETED_LL_NODES:
            result = cleanup_deleted_linked_list_nodes(args.sub_hash_bucket_ptr->linked_list_head);
            break;
        default:
            result = ERR_UNSUPPORTED_HASH_BUCKET_OP; // Error handling: unsupported operation type
            break;
    }

    // Release the lock
    lock_result = pthread_rwlock_unlock(&args.sub_hash_bucket_ptr->sub_hash_bucket_lock);
    if(lock_result != 0) return ERR_RW_LOCK_RELEASE_FAILED; // Error handling: failed to release lock

    return result;
}


int _lock_wrapper_for_data_node_operation(data_node_operation_t operation_type, data_node* data_node_ptr, key_value_pair* value)
{
    int lock_result = pthread_mutex_lock(&data_node_ptr->lock);
    if (lock_result != 0) return ERR_MUTEX_LOCK_ACQUIRE_FAILED; // Handle error: failed to acquire lock

    int result = 0;
    switch(operation_type) {
        case UPDATE_NODE:
            result = edit_data_node_value(data_node_ptr, value);
            break;
        case READ_NODE:
            result = read_data_node_value(data_node_ptr, value);
            break;
        case SOFT_DELETE_NODE:
            result = soft_delete_data_node(data_node_ptr);
            break;
        default:
            result = ERR_UNSUPPORTED_DATA_NODE_OP; // Invalid operation type
            break;
    }

    int unlock_result = pthread_mutex_unlock(&data_node_ptr->lock);
    if (unlock_result != 0) return ERR_MUTEX_LOCK_RELEASE_FAILED; // Handle error: failed to release lock

    return result;
}

int _check_for_resize_condition(sub_hash_bucket* sub_hash_bucket_ptr)
{
    if(sub_hash_bucket_ptr == NULL) return ERR_INVALID_ARGUMENT;

    if(sub_hash_bucket_ptr->total_node_count <= sub_hash_bucket_ptr->max_linked_list_chain_length) {
        return 0; // No resize needed
    }

    // if total_node_count and active_node_count are equal, no soft deleted nodes to reclaim - need to resize
    if(sub_hash_bucket_ptr->active_node_count == sub_hash_bucket_ptr->total_node_count) {
        return 21; // Indicate that resizing is needed
    }

    // There are soft deleted nodes that can be reclaimed
    int cleanup_result = (sub_hash_bucket_ptr->is_concurrency_enabled) ? _lock_wrapper_for_linked_list_node_operation(CLEANUP_DELETED_LL_NODES, (sub_hash_bucket_operation_args){.sub_hash_bucket_ptr = sub_hash_bucket_ptr, .key = NULL, .key_hash = 0}, NULL, NULL) : cleanup_deleted_linked_list_nodes(sub_hash_bucket_ptr->linked_list_head);

    if(cleanup_result < 0) {
        return cleanup_result; // Error handling: failed to cleanup deleted linked list nodes
    }

    sub_hash_bucket_ptr->total_node_count -= cleanup_result;

    // After cleanup, check if we still need to resize
    if(sub_hash_bucket_ptr->total_node_count >= 12) {
        return 22; // Indicate that resizing is needed
    }

    return 0; // No resize needed
}

#pragma endregion