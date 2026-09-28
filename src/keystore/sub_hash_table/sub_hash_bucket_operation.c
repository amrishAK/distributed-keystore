#include "sub_hash_bucket_operation.h"
#include "data_structures/linked_list_operation.h"
#include "data_structures/data_node_operation.h"
#include "data_structures/bloom_filter_operation.h"
#include "utils/memory_manager.h"
#include "operation_handlers/data_handler.h"
#include "operation_handlers/coordination_handler.h"

#include <stdio.h>
#include <stdatomic.h>

#pragma region private Concurrency Lock Wrapper Declarations
static int _check_for_resize_condition(sub_hash_bucket* sub_hash_bucket_ptr);
#pragma endregion

#pragma region Public Function Definitions

int initialise_sub_hash_bucket(sub_hash_bucket *sub_hash_bucket_ptr, bool is_concurrency_enabled, unsigned int max_linked_list_chain_length)
{
    if(sub_hash_bucket_ptr == NULL) return ERR_INVALID_ARGUMENT;

    if(sub_hash_bucket_ptr->is_initialized) return SUCCESS; // Already initialized, nothing to do

    sub_hash_bucket_ptr->is_bloom_filter_enabled = false; // Default to bloom filter disabled
    sub_hash_bucket_ptr->bloom_filter_ptr = NULL; // Default to no bloom filter pointer

    // Initialize bloom filter for the sub-hash-bucket if it does not already exist
    if(sub_hash_bucket_ptr->bloom_filter_ptr == NULL) {
        bloom_filter_t* bloom_filter = NULL;
        int bloom_filter_result = initialize_bloom_filter(max_linked_list_chain_length, &bloom_filter);
        
        if (bloom_filter_result == SUCCESS) {
            sub_hash_bucket_ptr->is_bloom_filter_enabled = true; // Bloom filter is enabled, set flag to true
            sub_hash_bucket_ptr->bloom_filter_ptr = bloom_filter;
        } 
        else if (bloom_filter_result == BLOOM_FILTER_DISABLED) {
            sub_hash_bucket_ptr->is_bloom_filter_enabled = false; // Bloom filter is disabled, set flag to false
            sub_hash_bucket_ptr->bloom_filter_ptr = NULL; // Bloom filter is disabled, set pointer to NULL
        } 
        else {
            return bloom_filter_result; // Error handling: failed to initialize bloom filter
        }

    }

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

    return SUCCESS;
}

int cleanup_sub_hash_bucket(sub_hash_bucket* sub_hash_bucket_ptr)
{
    if(sub_hash_bucket_ptr == NULL) return ERR_INVALID_ARGUMENT;
    if(!sub_hash_bucket_ptr->is_initialized) return 0; // Nothing to clean up

    int result = 0;
    sub_hash_bucket_operation_args args = {
        .sub_hash_bucket_ptr = sub_hash_bucket_ptr,
        .key = NULL,
        .key_hash = {0}
    };  
    
    result = delete_all_data_nodes(args); // Clean up all data nodes in the linked list

    if(sub_hash_bucket_ptr->is_concurrency_enabled) {
        int lock_destroy_result = pthread_rwlock_destroy(&sub_hash_bucket_ptr->sub_hash_bucket_lock);
        if (result == SUCCESS && lock_destroy_result != SUCCESS) {
            result = lock_destroy_result;
        }
    }

    if(sub_hash_bucket_ptr->bloom_filter_ptr != NULL) {
        cleanup_bloom_filter(sub_hash_bucket_ptr->bloom_filter_ptr);
        sub_hash_bucket_ptr->bloom_filter_ptr = NULL;
    }

    sub_hash_bucket_ptr->linked_list_head = NULL;
    sub_hash_bucket_ptr->is_initialized = false;
    sub_hash_bucket_ptr->active_node_count = 0;
    sub_hash_bucket_ptr->total_node_count = 0;
    sub_hash_bucket_ptr->is_concurrency_enabled = false;
    return result;
}

int update_node_in_sub_hash_bucket(sub_hash_bucket_operation_args args, key_value_pair* new_value)
{   
    if(new_value == NULL || args.sub_hash_bucket_ptr == NULL || args.key == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input

    int result = 0;
    data_node* target_data_node = NULL;
    result = find_data_node(args, &target_data_node);

    if (result != SUCCESS) return result; // Error handling: node not found

    result = update_data_node(target_data_node, args.sub_hash_bucket_ptr->is_concurrency_enabled, new_value);

    return result;
}

int add_node_to_sub_hash_bucket(sub_hash_bucket_operation_args args, key_value_pair* new_value)
{
    if(new_value == NULL || args.sub_hash_bucket_ptr == NULL || args.key == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input
    if(new_value->value_size == 0) return ERR_INVALID_ARGUMENT; // Error handling: zero-length value not allowed


    data_node* new_data_node = NULL;
    int result = create_data_node_from_value(args.key_hash, args.key, new_value->value, new_value->value_size, args.sub_hash_bucket_ptr->is_concurrency_enabled, &new_data_node);
    if (result != SUCCESS) return result; // Error handling: failed to create new data node

    result = add_data_node(args, new_data_node);
    if (result != SUCCESS) {
        return result;
    }

    atomic_fetch_add_explicit(&args.sub_hash_bucket_ptr->active_node_count, 1, memory_order_relaxed);
    atomic_fetch_add_explicit(&args.sub_hash_bucket_ptr->total_node_count, 1, memory_order_relaxed);

    result =  _check_for_resize_condition(args.sub_hash_bucket_ptr);

    // If no resize needed, return SUCESS_ADDED_NEW_NODE; 
    // otherwise pass through resize-triggered or error codes as-is
    return result == SUCCESS ? SUCESS_ADDED_NEW_NODE : result; 
}

int get_key_store_value_from_sub_hash_bucket(sub_hash_bucket_operation_args args, key_value_pair* value_out)
{
    if(args.sub_hash_bucket_ptr == NULL || args.key == NULL || value_out == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input

    data_node* target_data_node = NULL;
    int result = find_data_node(args, &target_data_node);
    if (result != SUCCESS) return result; // Error handling: node not found

    result = get_data_node_value(target_data_node, target_data_node->is_concurrency_enabled, value_out);

    return result;
}

int delete_key_from_sub_hash_bucket(sub_hash_bucket_operation_args args)
{
    if(args.sub_hash_bucket_ptr == NULL || args.key == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input

    data_node* target_data_node = NULL;
    int result = find_data_node(args, &target_data_node);

    if (result != SUCCESS) return result; // Error handling: node not found

    result = soft_delete(target_data_node, args.sub_hash_bucket_ptr->is_concurrency_enabled);

    if (result == SUCCESS) {
        atomic_fetch_sub_explicit(&args.sub_hash_bucket_ptr->active_node_count, 1, memory_order_relaxed);
    }

    return result;
}

int is_node_in_sub_hash_bucket(sub_hash_bucket_operation_args args)
{
    if(args.sub_hash_bucket_ptr == NULL || args.key == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input

    // Use full lookup so behavior stays correct whether bloom filter is enabled or not.
    data_node* target_data_node = NULL;
    int result = find_data_node(args, &target_data_node);
    return (result == SUCCESS) ? SUCCESS : ERR_DATA_NODE_NOT_FOUND;
}


#pragma endregion


#pragma region Private Concurrency Lock Wrapper Definitions

int _check_for_resize_condition(sub_hash_bucket* sub_hash_bucket_ptr)
{
    if(sub_hash_bucket_ptr == NULL) return ERR_INVALID_ARGUMENT;

    // Check if the total node count exceeds the maximum linked list chain length threshold
    unsigned int total_node_count = atomic_load_explicit(&sub_hash_bucket_ptr->total_node_count, memory_order_relaxed);
    if(total_node_count <= sub_hash_bucket_ptr->max_linked_list_chain_length) {
        return SUCCESS; // No resize needed
    }

    // if total_node_count and active_node_count are equal, no soft deleted nodes to reclaim - need to resize
    unsigned int active_node_count = atomic_load_explicit(&sub_hash_bucket_ptr->active_node_count, memory_order_relaxed);
    if(active_node_count == total_node_count) {
        return SUCESS_ADDED_NEW_NODE_RESZING_TRIGGERED; // Indicate that resizing is needed
    }

    // There are soft deleted nodes that can be reclaimed
    sub_hash_bucket_operation_args args = (sub_hash_bucket_operation_args){.sub_hash_bucket_ptr = sub_hash_bucket_ptr, .key = NULL, .key_hash = {0}};
    unsigned int deleted_count = 0;
    int cleanup_result = cleanup_deleted_data_nodes(args, &deleted_count); // Clean up soft deleted nodes to try to reclaim memory and avoid resizing

    if(cleanup_result < 0) {
        return cleanup_result; // Error handling: failed to cleanup deleted linked list nodes
    }

    // Update total node count after cleanup
    total_node_count = atomic_fetch_sub_explicit(&sub_hash_bucket_ptr->total_node_count, deleted_count, memory_order_relaxed) - deleted_count;

    // After cleanup, check if we still need to resize
    if(total_node_count >= sub_hash_bucket_ptr->max_linked_list_chain_length) {
        return SUCESS_ADDED_NEW_NODE_RESZING_TRIGGERED; // Indicate that resizing is needed
    }

    return SUCCESS; // No resize needed
}

#pragma endregion