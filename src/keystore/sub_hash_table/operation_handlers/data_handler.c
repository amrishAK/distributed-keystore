#include "data_handler.h"
#include "data_structures/data_node_operation.h"

#pragma region private Concurrency Lock Wrapper Declarations
static int _lock_wrapper_for_data_node_operation(data_node_operation_t operation_type, data_node* data_node_ptr, key_value_pair* value);
#pragma endregion



#pragma region Public Function Definitions

int create_data_node_from_value(const composite_key_hash key_hash, const char* key, const unsigned char* value, const size_t value_size, const bool is_concurrency_enabled, data_node** new_node_out)
{
    key_value_pair kv_pair = {
        .key = (char*)key,
        .value = (unsigned char*)value,
        .value_size = value_size
    };

    return create_new_data_node(key_hash, &kv_pair, is_concurrency_enabled, new_node_out);
}

int get_data_node_value(const data_node* data_node_ptr, const bool is_concurrency_enabled, key_value_pair* value_out)
{
    return (is_concurrency_enabled) ? _lock_wrapper_for_data_node_operation(READ_NODE, (data_node*)data_node_ptr, value_out) : read_data_node_value((data_node*)data_node_ptr, value_out);
}

int update_data_node(data_node* data_node_ptr, const bool is_concurrency_enabled , const key_value_pair* new_value)
{
    return (is_concurrency_enabled) ? _lock_wrapper_for_data_node_operation(UPDATE_NODE, data_node_ptr, (key_value_pair*)new_value) : edit_data_node_value(data_node_ptr, (key_value_pair*)new_value);
}

int soft_delete(data_node* data_node_ptr, const bool is_concurrency_enabled)
{
    return (is_concurrency_enabled) ? _lock_wrapper_for_data_node_operation(SOFT_DELETE_NODE, data_node_ptr, NULL) : soft_delete_data_node(data_node_ptr);
}

#pragma endregion



#pragma region Private Concurrency Lock Wrapper Definitions
/** 
 * @fn _lock_wrapper_for_data_node_operation
 * @brief Concurrency lock wrapper for data node operations.
 * Acquires the mutex lock on the data node, performs the specified operation (read, update, or soft delete), and releases the lock.
 * @param operation_type The type of data node operation to perform (READ_NODE, UPDATE_NODE, SOFT_DELETE_NODE).
 * @param data_node_ptr Pointer to the data node on which to perform the operation.
 * @param value Pointer to a key_value_pair structure for read/write operations (ignored for soft delete).
 * @return int Returns the result of the data node operation.
 */
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
    
    // Handle unlock error: if the operation failed, return that result; otherwise, return unlock error
    if (unlock_result != SUCCESS) {
        if (result != SUCCESS) return result; // Return the original operation result if it failed
        return ERR_MUTEX_LOCK_RELEASE_FAILED; // Handle error: failed to release lock
    }

    return result;
}

#pragma endregion
