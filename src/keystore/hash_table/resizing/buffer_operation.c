#include "buffer_operation.h"
#include "sub_hash_table/sub_hash_table_operation.h"
#include "data_structures/double_linked_list_operation.h"
#include "data_structures/linked_list_operation.h"
#include "data_structures/data_node_operation.h"
#include "sub_hash_table/sub_hash_table_operation.h"
#include "utils/memory_manager.h"
#include "utils/background_task_manager.h"
#include "utils/helper_functions.h"

#include <stdio.h>
#include <string.h>


#pragma region  Private Function Declarations
static int _init_new_operation_buffer(new_operation_buffer** buffer_out);
static int _init_delete_operation_buffer(delete_operation_buffer** buffer_out, bool is_reallocating);
static int _free_delete_operation_buffer(delete_operation_buffer* buffer_ptr);
static int _free_new_operation_buffer(new_operation_buffer* buffer_ptr);
static int _find_node_in_delete_operation_buffer(delete_operation_buffer* delete_operation_buffer_ptr, composite_key_hash key_hash, const char* key);
static int _commit_data_node_operation(data_node* data_node_ptr, sub_hash_table_memory_pool* target_sub_hash_table_ptr);
static int _process_new_operation_buffer(new_operation_buffer* new_operation_buffer_ptr, sub_hash_table_memory_pool* target_sub_hash_table_ptr, bool is_target_snapshot);
static int _process_updated_operation_buffer(linked_list_node* updated_operation_buffer_head, sub_hash_table_memory_pool* target_sub_hash_table_ptr);
static int _process_deleted_operation_buffer(delete_operation_buffer* delete_operation_buffer_ptr, sub_hash_table_memory_pool* target_sub_hash_table_ptr);
#pragma endregion

#pragma region Public Function Definitions

int chase_buffer_worker(void* input_arg)
{
    if (input_arg == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input

    background_task_args_t* args = (background_task_args_t*)input_arg;

    resizing_buffer* resizing_buffer_ptr = (resizing_buffer*)args->task_input_args;
    new_operation_buffer* new_operation_buffer_ptr = resizing_buffer_ptr->new_operation_buffer_ptr;
    sub_hash_table_memory_pool* target_sub_hash_table_ptr = resizing_buffer_ptr->new_sub_hash_table_ptr;

    
    
    if(new_operation_buffer_ptr == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input
    if(target_sub_hash_table_ptr == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input
    
    double_linked_list_node* consumer_ptr = NULL;
    double_linked_list_node* head_ptr = NULL;
    double_linked_list_node* tail_ptr = NULL;

    while (true) // Continue processing until kill signal is set
    {   
        if (atomic_load(&args->kill_signal))
        {
            break; // Exit the loop to terminate the worker
        }

        // Lock the buffer to safely read the current head, tail, and consumer pointers
        pthread_spin_lock(&new_operation_buffer_ptr->buffer_lock);
        consumer_ptr = new_operation_buffer_ptr->current_consumer_ptr;
        head_ptr = new_operation_buffer_ptr->head_ptr;
        tail_ptr = new_operation_buffer_ptr->tail_ptr;
        pthread_spin_unlock(&new_operation_buffer_ptr->buffer_lock);

        if(head_ptr == NULL || tail_ptr == NULL) {
            portable_sleep_us(100); continue;
        }
    
        consumer_ptr = consumer_ptr ? consumer_ptr->prev_node_ptr : tail_ptr;
        if(consumer_ptr == NULL || consumer_ptr == head_ptr) {
            portable_sleep_us(100); continue;
        }

        int result  = _commit_data_node_operation(consumer_ptr->data_node_ptr, target_sub_hash_table_ptr);

        

        pthread_spin_lock(&new_operation_buffer_ptr->buffer_lock);
        new_operation_buffer_ptr->current_consumer_ptr = consumer_ptr->prev_node_ptr; // Move the consumer pointer to the next node to be processed
        pthread_spin_unlock(&new_operation_buffer_ptr->buffer_lock);

        if(result != SUCCESS) return result; // Propagate error if commit operation failed

        portable_sleep_us(100);
    }

    

    return SUCCESS;
}

int initialize_chase_worker(resizing_buffer* resizing_buffer_ptr, uint32_t* out_task_uuid)
{
    if (resizing_buffer_ptr == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input
    if (out_task_uuid == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input

    // Initialize consumer pointer to NULL to start processing from the tail of the buffer
    resizing_buffer_ptr->new_operation_buffer_ptr->current_consumer_ptr = NULL;

    int result = initialize_background_function(chase_buffer_worker, (void*)resizing_buffer_ptr, false, out_task_uuid);
    
    return result;
}

int wait_for_chase_worker_to_finish(resizing_buffer* resizing_buffer_ptr,uint32_t task_uuid)
{
    if (resizing_buffer_ptr == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input 
    if (task_uuid == 0) return ERR_INVALID_ARGUMENT; // Error handling: invalid input
    int result = cleanup_background_task(task_uuid);
    resizing_buffer_ptr->new_operation_buffer_ptr->current_consumer_ptr = NULL;  // invalidate snapshots
    
    return result;
}

int initialize_resizing_buffer(hash_bucket* hash_bucket_ptr)
{
    if(hash_bucket_ptr == NULL) return ERR_INVALID_ARGUMENT;

    // Allocate memory for resizing buffer and its new operation buffer
    hash_bucket_ptr->resizing_buffer_ptr = callocate_memory(1, sizeof(resizing_buffer));
    if(hash_bucket_ptr->resizing_buffer_ptr == NULL) return ERR_MEMORY_ALLOCATION_FAILED;

    int result = _init_new_operation_buffer(&hash_bucket_ptr->resizing_buffer_ptr->new_operation_buffer_ptr);
    if(result != SUCCESS)
    {
        free_memory(hash_bucket_ptr->resizing_buffer_ptr, false);
        hash_bucket_ptr->resizing_buffer_ptr = NULL;
        return result;
    }

    result = _init_delete_operation_buffer(&hash_bucket_ptr->resizing_buffer_ptr->delete_operation_buffer_ptr, false);
    if(result != SUCCESS)
    {
        free_memory(hash_bucket_ptr->resizing_buffer_ptr->new_operation_buffer_ptr, false);
        free_memory(hash_bucket_ptr->resizing_buffer_ptr, false);
        hash_bucket_ptr->resizing_buffer_ptr = NULL;
        return result;
    }

    return SUCCESS;
}


int insert_node_to_new_operation_buffer(hash_bucket* hash_bucket_ptr, composite_key_hash key_hash, key_value_pair* kv_pair, bool is_delete_operation)
{
    if (hash_bucket_ptr == NULL || kv_pair == NULL) return ERR_INVALID_ARGUMENT;

    int result = 0;
    data_node* new_data_node = NULL;

    result = create_new_data_node( key_hash, kv_pair, hash_bucket_ptr->sub_hash_table_config.is_concurrency_enabled, &new_data_node);
    if (result != SUCCESS) return result;

    if (is_delete_operation) new_data_node->is_deleted = true;

    double_linked_list_node* new_node = NULL;
    result = create_new_double_linked_list_node(key_hash, new_data_node, &new_node);
    if (result != SUCCESS) return result;

    new_operation_buffer* new_operation_buffer = hash_bucket_ptr->resizing_buffer_ptr->new_operation_buffer_ptr;

    pthread_spin_lock(&new_operation_buffer->buffer_lock);
    result = insert_double_linked_list_node(&new_operation_buffer->head_ptr, new_node);
    // tail is the oldest node — set only on first insert, never changes
    if (new_operation_buffer->tail_ptr == NULL) new_operation_buffer->tail_ptr = new_node;
    
    pthread_spin_unlock(&new_operation_buffer->buffer_lock);

    return (result == SUCCESS) ? SUCCESS_ADDED_TO_PENDING_LIST : result;
}

int delete_resizing_buffer(resizing_buffer* resizing_buffer_ptr)
{
    if(resizing_buffer_ptr == NULL) return ERR_INVALID_ARGUMENT;

    int result = SUCCESS;
    int temp_result = SUCCESS;

    // Delete updated operation buffer
    temp_result = delete_all_linked_list_nodes(&resizing_buffer_ptr->updated_operation_buffer_head);
    if(temp_result != SUCCESS) result = temp_result;
    resizing_buffer_ptr->updated_operation_buffer_head = NULL;

    // Delete deleted operation buffer
    _free_delete_operation_buffer(resizing_buffer_ptr->delete_operation_buffer_ptr);
    resizing_buffer_ptr->delete_operation_buffer_ptr = NULL;

    // Delete operation list buffer
    _free_new_operation_buffer(resizing_buffer_ptr->new_operation_buffer_ptr);
    resizing_buffer_ptr->new_operation_buffer_ptr = NULL;

    //cleanup new sub-hash-table if it exists (in case resizing was not finalized and new sub-hash-table was never swapped in)
    if(resizing_buffer_ptr->new_sub_hash_table_ptr != NULL) {
        cleanup_sub_hash_table(resizing_buffer_ptr->new_sub_hash_table_ptr);
        free_memory(resizing_buffer_ptr->new_sub_hash_table_ptr, false);
        resizing_buffer_ptr->new_sub_hash_table_ptr = NULL;
    }
    return result;
}

int insert_delete_operation_to_resizing_buffer(hash_bucket *hash_bucket_ptr, composite_key_hash key_hash, const char *key)
{
    if(hash_bucket_ptr == NULL || key == NULL) return ERR_INVALID_ARGUMENT;

    resizing_buffer* resizing_buffer_ptr = hash_bucket_ptr->resizing_buffer_ptr;

    // Initialize delete operation buffer if it hasn't been initialized yet
    if(resizing_buffer_ptr->delete_operation_buffer_ptr == NULL) {
        int result = _init_delete_operation_buffer(&resizing_buffer_ptr->delete_operation_buffer_ptr, false);
        if(result != SUCCESS) return result;
    }

    // Check if we need to reallocate the delete operation buffer (if count has reached capacity)
    if(resizing_buffer_ptr->delete_operation_buffer_ptr->count >= resizing_buffer_ptr->delete_operation_buffer_ptr->capacity) {
        int result = _init_delete_operation_buffer(&resizing_buffer_ptr->delete_operation_buffer_ptr, true);
        if(result != SUCCESS) return result;
    }

    //create new delete operation
    delete_operation new_delete_operation;
    new_delete_operation.key = callocate_memory(strlen(key) + 1, sizeof(char));
    if(new_delete_operation.key == NULL) {
        return ERR_MEMORY_ALLOCATION_FAILED;
    }
    strcpy(new_delete_operation.key, key);
    new_delete_operation.key_hash = key_hash;

    // Insert new delete operation into buffer
    resizing_buffer_ptr->delete_operation_buffer_ptr->operations[resizing_buffer_ptr->delete_operation_buffer_ptr->count] = new_delete_operation;
    resizing_buffer_ptr->delete_operation_buffer_ptr->count += 1;

    return SUCCESS;
}

int insert_update_operation_to_resizing_buffer(hash_bucket *hash_bucket_ptr, composite_key_hash key_hash, key_value_pair *kv_pair)
{
    if(hash_bucket_ptr == NULL || kv_pair == NULL) return ERR_INVALID_ARGUMENT;
    if(kv_pair->value == NULL || kv_pair->value_size == 0) return ERR_INVALID_ARGUMENT;
    
    int result = 0;
    data_node* new_data_node = NULL;
    result = create_new_data_node(key_hash, kv_pair, hash_bucket_ptr->sub_hash_table_config.is_concurrency_enabled, &new_data_node);
    if(result != SUCCESS) return result;

    linked_list_node* new_node = NULL;
    result = create_new_linked_list_node(key_hash, new_data_node, &new_node);
    if(result != SUCCESS) return result;

    return insert_linked_list_node(&hash_bucket_ptr->resizing_buffer_ptr->updated_operation_buffer_head, new_node);
}

int get_node_from_resizing_buffer(hash_bucket *hash_bucket_ptr, composite_key_hash key_hash, const char *key, bool ignore_current_operation_buffer, key_value_pair *kv_pair_out)
{
    if(hash_bucket_ptr == NULL || key == NULL || kv_pair_out == NULL) return ERR_INVALID_ARGUMENT;

    int result = 0;
    data_node* data_node_found = NULL;

    if(ignore_current_operation_buffer)
    {
        // Check deleted buffer first
        result = _find_node_in_delete_operation_buffer(hash_bucket_ptr->resizing_buffer_ptr->delete_operation_buffer_ptr, key_hash, key);

        if(result == SUCCESS) {
            return ERR_DATA_NODE_NOT_FOUND; // Node is marked for deletion, treat as not found
        }
        
        // If not found, check updated buffer
        result = get_data_node_from_linked_list(hash_bucket_ptr->resizing_buffer_ptr->updated_operation_buffer_head, key, key_hash, false, &data_node_found);
        if(result == SUCCESS && data_node_found != NULL) {
            read_data_node_value(data_node_found, kv_pair_out);
            return SUCCESS;
        }
        return ERR_DATA_NODE_NOT_FOUND;
    }
    else
    {
        result = get_key_store_value_from_sub_hash_table(hash_bucket_ptr->resizing_buffer_ptr->new_sub_hash_table_ptr, key_hash, key, kv_pair_out);
        if(result == SUCCESS) {
            return SUCCESS;
        }
        // If not found in sub-hash-table, check operation list
        result = find_data_node_in_double_linked_list(hash_bucket_ptr->resizing_buffer_ptr->new_operation_buffer_ptr->head_ptr, key, key_hash, true, &data_node_found);
        if(result == SUCCESS && data_node_found != NULL) {
            read_data_node_value(data_node_found, kv_pair_out);
            return SUCCESS;
        }
        return ERR_DATA_NODE_NOT_FOUND;
    }
}


int commit_resizing_buffer_operations_to_sub_hash_table(hash_bucket* hash_bucket_ptr)
{
    if(hash_bucket_ptr == NULL || hash_bucket_ptr->resizing_buffer_ptr == NULL) return ERR_INVALID_ARGUMENT;

    // Determine target sub-hash-table for committing operations: If new sub-hash-table exists, commit to it; otherwise, commit to snapshot sub-hash-table
    bool is_target_snapshot = false;
    
    if(hash_bucket_ptr->resizing_buffer_ptr->new_sub_hash_table_ptr == NULL) {
        is_target_snapshot = true;
    }

    

    sub_hash_table_memory_pool* target_sub_hash_table_ptr = (is_target_snapshot) ? hash_bucket_ptr->snapshot_sub_hash_table_ptr : hash_bucket_ptr->resizing_buffer_ptr->new_sub_hash_table_ptr;
    if (target_sub_hash_table_ptr == NULL) return ERR_SUB_HASH_TABLE_NOT_INITIALIZED;

    int result = 0;

    
    
    result = _process_new_operation_buffer(hash_bucket_ptr->resizing_buffer_ptr->new_operation_buffer_ptr, target_sub_hash_table_ptr, is_target_snapshot);
    if(result != SUCCESS) return result;

    if(hash_bucket_ptr->resizing_buffer_ptr->updated_operation_buffer_head != NULL) {
        result = _process_updated_operation_buffer(hash_bucket_ptr->resizing_buffer_ptr->updated_operation_buffer_head, target_sub_hash_table_ptr);
        if(result != SUCCESS) return result;
    }

    if(hash_bucket_ptr->resizing_buffer_ptr->delete_operation_buffer_ptr != NULL &&
       hash_bucket_ptr->resizing_buffer_ptr->delete_operation_buffer_ptr->count > 0) {
        result = _process_deleted_operation_buffer(hash_bucket_ptr->resizing_buffer_ptr->delete_operation_buffer_ptr, target_sub_hash_table_ptr);
        if(result != SUCCESS) return result;
    }

    return SUCCESS;
}

#pragma endregion


#pragma region Private Function Definitions

int _init_new_operation_buffer(new_operation_buffer** buffer_out)
{
    if(buffer_out == NULL) return ERR_INVALID_ARGUMENT;

    new_operation_buffer* new_buffer = callocate_memory(1, sizeof(new_operation_buffer));
    if(new_buffer == NULL) return ERR_MEMORY_ALLOCATION_FAILED;

    int result = pthread_spin_init(&new_buffer->buffer_lock, PTHREAD_PROCESS_PRIVATE);

    if( result != 0) {
        free_memory(new_buffer, false);
        return ERR_MEMORY_ALLOCATION_FAILED;
    }

    new_buffer->head_ptr = NULL;
    new_buffer->current_consumer_ptr = NULL;
    new_buffer->pause_chasing = false;

    *buffer_out = new_buffer;
    return SUCCESS;
}

int _init_delete_operation_buffer(delete_operation_buffer** buffer_out, bool is_reallocating)
{
    if(buffer_out == NULL) return ERR_INVALID_ARGUMENT;

    delete_operation_buffer* new_buffer = NULL;

    new_buffer = (is_reallocating) ? *buffer_out : callocate_memory(1, sizeof(delete_operation_buffer));
    if(new_buffer == NULL) return ERR_MEMORY_ALLOCATION_FAILED;

    new_buffer->count = (is_reallocating) ? new_buffer->count : 0; // If reallocating, keep the current count, otherwise initialize to 0

    new_buffer->capacity = (is_reallocating) ? new_buffer->capacity + 500 : 500; // If reallocating, increase capacity by 500, otherwise initialize to 500

    delete_operation* new_operations = (is_reallocating) ? reallocate_memory(new_buffer->operations, new_buffer->capacity * sizeof(delete_operation)) : callocate_memory(new_buffer->capacity, sizeof(delete_operation));

    if(new_operations == NULL) {
        if(!is_reallocating) {
            free_memory(new_operations, false);
            free_memory(new_buffer, false);
        }
        return ERR_MEMORY_ALLOCATION_FAILED;
    }

    new_buffer->operations = new_operations;
    *buffer_out = new_buffer;
    return SUCCESS;
}

int _free_delete_operation_buffer(delete_operation_buffer* buffer_ptr)
{
    if(buffer_ptr == NULL) return ERR_INVALID_ARGUMENT;

    for(unsigned int i = 0; i < buffer_ptr->count; i++)
    {
        free_memory(buffer_ptr->operations[i].key, false);
    }
    free_memory(buffer_ptr->operations, false);
    free_memory(buffer_ptr, false);

    return SUCCESS;
}

int _free_new_operation_buffer(new_operation_buffer* buffer_ptr)
{
    if(buffer_ptr == NULL) return SUCCESS;

    delete_double_linked_list(buffer_ptr->head_ptr);
    buffer_ptr->head_ptr = NULL;
    buffer_ptr->current_consumer_ptr = NULL;
    buffer_ptr->tail_ptr = NULL;

    pthread_spin_destroy(&buffer_ptr->buffer_lock);
    free_memory(buffer_ptr, false);

    return SUCCESS;
}

int _find_node_in_delete_operation_buffer(delete_operation_buffer* delete_operation_buffer_ptr, composite_key_hash key_hash, const char* key)
{
    if(delete_operation_buffer_ptr == NULL || key == NULL) return ERR_INVALID_ARGUMENT;

    for(unsigned int i = 0; i < delete_operation_buffer_ptr->count; i++)
    {
        if(delete_operation_buffer_ptr->operations[i].key_hash.sub_bucket_hash == key_hash.sub_bucket_hash)
        {
            if(strcmp(delete_operation_buffer_ptr->operations[i].key, key) == 0) return SUCCESS; // Node is marked for deletion
        }
    }

    return ERR_DATA_NODE_NOT_FOUND; // Node is not marked for deletion
}

int _commit_data_node_operation(data_node* data_node_ptr, sub_hash_table_memory_pool* target_sub_hash_table_ptr)
{
    if(data_node_ptr == NULL || target_sub_hash_table_ptr == NULL) return ERR_INVALID_ARGUMENT;

    if(data_node_ptr->is_deleted) {
        
        return delete_key_from_sub_hash_table(target_sub_hash_table_ptr, data_node_ptr->key_hash, data_node_ptr->key);
    }
    else {
        key_value_pair kv_pair = {
            .key = data_node_ptr->key,
            .value = data_node_ptr->data,
            .value_size = data_node_ptr->data_size
        };
        
        return upsert_node_to_sub_hash_table(target_sub_hash_table_ptr, data_node_ptr->key_hash, &kv_pair);
    }
}

/**
 * @fn _process_new_operation_buffer
 * @brief Process the new operation buffer by applying all operations in the buffer to the target sub-hash-table. 
 * If is_target_snapshot is true, process up to and including the current consumer pointer; otherwise, process all nodes in the buffer.
 * @param new_operation_buffer_ptr Pointer to the new operation buffer to be processed.
 * @param target_sub_hash_table_ptr Pointer to the target sub-hash-table where the operations should be applied.
 * @param is_target_snapshot Boolean flag indicating whether the target sub-hash-table is the snapshot sub-hash-table (true) or the new sub-hash-table (false). If true, only process up to and including the current consumer pointer; if false, process all nodes in the buffer.
 * @return int Returns 0 on success, or a negative value on failure.
 */
int _process_new_operation_buffer(new_operation_buffer* new_operation_buffer_ptr, sub_hash_table_memory_pool* target_sub_hash_table_ptr, bool is_target_snapshot)
{
    if(new_operation_buffer_ptr == NULL) return ERR_INVALID_ARGUMENT;

    double_linked_list_node* head_ptr = new_operation_buffer_ptr->head_ptr;
    double_linked_list_node* current_consumer_ptr = new_operation_buffer_ptr->current_consumer_ptr;
    double_linked_list_node* tail_ptr = new_operation_buffer_ptr->tail_ptr;

    int result = SUCCESS;

    // If target is snapshot, commit operations from tail up to head. Else (target is new sub-hash-table), commit all operations from consumer pointer up to head
    current_consumer_ptr = (is_target_snapshot) ? current_consumer_ptr : tail_ptr; 

    // If consumer pointer is NULL, start from the tail (oldest operation)
    current_consumer_ptr = (current_consumer_ptr == NULL) ? tail_ptr : current_consumer_ptr; 

    while(current_consumer_ptr != NULL)
    {
        result = _commit_data_node_operation(current_consumer_ptr->data_node_ptr, target_sub_hash_table_ptr);
        
        if(result != SUCCESS && result != SUCESS_ADDED_NEW_NODE_RESZING_TRIGGERED && result != SUCESS_ADDED_NEW_NODE) return result;

        if(current_consumer_ptr == head_ptr) break; // If we've reached the head of the buffer, exit the loop
        if(current_consumer_ptr->prev_node_ptr == NULL) break; // Safety check to prevent dereferencing NULL pointer
        
        current_consumer_ptr = current_consumer_ptr->prev_node_ptr; // Move towards the head of the buffer
    }

    return SUCCESS;
}


int _process_updated_operation_buffer(linked_list_node* updated_operation_buffer_head, sub_hash_table_memory_pool* target_sub_hash_table_ptr)
{
    if(updated_operation_buffer_head == NULL) return ERR_INVALID_ARGUMENT;
    if(target_sub_hash_table_ptr == NULL) return ERR_INVALID_ARGUMENT;

    linked_list_node* current_node = updated_operation_buffer_head;

    while(current_node != NULL)
    {
        key_value_pair temp_kv_pair = {
            .key = current_node->data_node_ptr->key,
            .value = current_node->data_node_ptr->data,
            .value_size = current_node->data_node_ptr->data_size
        };

        int result = upsert_node_to_sub_hash_table(target_sub_hash_table_ptr, current_node->data_node_ptr->key_hash, &temp_kv_pair);
        if(result != SUCCESS && result != SUCESS_ADDED_NEW_NODE) return result;

        current_node = current_node->next_node_ptr;
    }

    return SUCCESS;
}

int _process_deleted_operation_buffer(delete_operation_buffer* delete_operation_buffer_ptr, sub_hash_table_memory_pool* target_sub_hash_table_ptr)
{
    if(delete_operation_buffer_ptr == NULL) return ERR_INVALID_ARGUMENT;
    if(target_sub_hash_table_ptr == NULL) return ERR_INVALID_ARGUMENT;

    for(unsigned int i = 0; i < delete_operation_buffer_ptr->count; i++)
    {
        int result = delete_key_from_sub_hash_table(target_sub_hash_table_ptr, delete_operation_buffer_ptr->operations[i].key_hash, delete_operation_buffer_ptr->operations[i].key);
        if(result != SUCCESS) return result;
    }

    return SUCCESS;
}

#pragma endregion