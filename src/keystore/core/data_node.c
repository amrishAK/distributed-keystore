#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "data_node.h"
#include "core/type_definition.h"
#include "utils/memory_manager.h"

#pragma region Private Function Declarations
int _allocate_and_init_data_node(size_t key_len, bool is_concurrency_enabled, data_node** data_node_ptr);
int _add_data_to_node(data_node *node_ptr, key_store_value* value);
int _add_key_to_node(data_node *node_ptr, const char *key, size_t key_len, uint32_t key_hash);
int _update_data_node(data_node *node_ptr, key_store_value* new_value);
int _operate_data_node_counters(data_node_operation_type_t operation_type, int operation_result);

#pragma endregion

#pragma region global Variables
data_node_operation_counters g_data_node_operation_counters = {0};
#pragma endregion

#pragma region Public Function Definitions

int create_data_node(const char *key, uint32_t key_hash, key_store_value* value, bool is_concurrency_enabled, data_node** data_node_ptr) 
{
    // Argument validation
    if (key == NULL || key[0] == '\0' || value == NULL || value->data == NULL || value->data_size == 0) return _operate_data_node_counters(DATA_NODE_CREATE, -20); // Handle invalid input

    // Allocation and initialisation
    size_t key_len = strlen(key) + 1;
    data_node* node = NULL;
    int alloc_result = _allocate_and_init_data_node(key_len, is_concurrency_enabled, &node);
    if (alloc_result != 0) return _operate_data_node_counters(DATA_NODE_CREATE, alloc_result);

    // Add key to node
    if(_add_key_to_node(node, key, key_len, key_hash) != 0) {
       delete_data_node(node);
       return _operate_data_node_counters(DATA_NODE_CREATE, -48);
    }

    // Add data to node
    if (_add_data_to_node(node, value) != 0) {
       delete_data_node(node);
       return _operate_data_node_counters(DATA_NODE_CREATE, -48);
    }
    
    *data_node_ptr  = node;
    return _operate_data_node_counters(DATA_NODE_CREATE, 0);
}


int update_data_node(data_node *node_ptr, key_store_value* new_value) {

    if (node_ptr == NULL || new_value == NULL || new_value->data == NULL) return _operate_data_node_counters(DATA_NODE_UPDATE, -20); // Handle null pointer
    int result = _update_data_node(node_ptr, new_value);
    return _operate_data_node_counters(DATA_NODE_UPDATE, result);
}

int delete_data_node(data_node *node_ptr) {

    int result = 0;
    if (node_ptr == NULL) return _operate_data_node_counters(DATA_NODE_DELETE, -20); // Handle null pointer, nothing to delete

    free_memory(node_ptr->data, NO_POOL);
    
    if(node_ptr->is_concurrency_enabled){
        result = pthread_mutex_destroy(&node_ptr->lock);
    }

    free_memory(node_ptr, NO_POOL);

    return _operate_data_node_counters(DATA_NODE_DELETE, result);
}

int get_data_from_node(data_node *node_ptr, key_store_value *value_out) {
    if (node_ptr == NULL || value_out == NULL) return _operate_data_node_counters(DATA_NODE_READ, -20); // Handle null pointer
    
    if (node_ptr->data_size == 0 || node_ptr->data == NULL) {
        value_out->data = NULL;
        value_out->data_size = 0;
        return _operate_data_node_counters(DATA_NODE_READ, 0);
    }

    value_out->data = (unsigned char *)allocate_memory(node_ptr->data_size);
    if (value_out->data == NULL) return _operate_data_node_counters(DATA_NODE_READ, -10); // Handle memory allocation failure

    memcpy(value_out->data, node_ptr->data, node_ptr->data_size);
    value_out->data_size = node_ptr->data_size;

    return _operate_data_node_counters(DATA_NODE_READ, 0);
}

int data_node_mutex_lock_wrapper(data_node_operation_type_t operation_type, data_node* data_node_ptr, key_store_value* value) {
    if(data_node_ptr == NULL) return _operate_data_node_counters(operation_type, -20); // Handle null pointer
    int result = 0;
    int lock_result = 0;

    lock_result = pthread_mutex_lock(&data_node_ptr->lock);
    if (lock_result != 0) return _operate_data_node_counters(operation_type, -30); // Handle error: failed to acquire lock

    switch(operation_type) {
        case DATA_NODE_UPDATE:
            result = update_data_node(data_node_ptr, value);
            break;
        case DATA_NODE_READ:
            result = get_data_from_node(data_node_ptr, value);
            break;
        default:
            result = -47; // Invalid operation type
            break;
    }

    int unlock_result = pthread_mutex_unlock(&data_node_ptr->lock);
    if (unlock_result != 0) return _operate_data_node_counters(operation_type, -31); // Handle error: failed to release lock

    return result;
}

data_node_operation_counters get_data_node_operation_counters(void) {
    data_node_operation_counters counters_copy;
    memcpy(&counters_copy, &g_data_node_operation_counters, sizeof(data_node_operation_counters));
    return counters_copy;
}

#pragma endregion

#pragma region Private Function Definitions

/**
 * @fn _operate_data_node_counters
 * @brief Updates the global data node operation counters based on the operation type and result.
 *
 * This function increments the appropriate counters in the global
 * data_node_operation_counters structure based on the operation type
 * (update, read, delete, create) and whether the operation was successful or failed.
 *
 * @param operation_type The type of operation performed.
 * @param operation_result The result of the operation (0 for success, non-zero for failure).
 * @return int Returns the original operation_result for convenience.
 */
int _operate_data_node_counters(data_node_operation_type_t operation_type, int operation_result) {
    switch (operation_type) {
        case DATA_NODE_UPDATE:
            g_data_node_operation_counters.total_update_ops++;
            if (operation_result != 0) g_data_node_operation_counters.failed_update_ops++;
            break;
        case DATA_NODE_READ:
            g_data_node_operation_counters.total_read_ops++;
            if (operation_result != 0) g_data_node_operation_counters.failed_read_ops++;
            break;
        case DATA_NODE_DELETE:
            g_data_node_operation_counters.total_delete_ops++;
            if (operation_result != 0) g_data_node_operation_counters.failed_delete_ops++;
            break;
        case DATA_NODE_CREATE:
            g_data_node_operation_counters.total_create_ops++;
            if (operation_result != 0) g_data_node_operation_counters.failed_create_ops++;
            break;
        default:
            break;
    }

    if (operation_result < 0 && operation_result > -100) {
        g_data_node_operation_counters.error_code_counters[-operation_result]++;
    }

    return operation_result;
}

/**
 * @fn _allocate_and_init_data_node
 * @brief Allocates memory for a data node and initializes its fields.
 *
 * This function allocates memory for a data_node structure including space for the key.
 * It also initializes the concurrency control mutex if enabled.
 *
 * @param key_len Length of the key including null terminator.
 * @param is_concurrency_enabled Flag indicating if concurrency control is enabled.
 * @return data_node* Pointer to the allocated and initialized data_node, or NULL on failure.
 */
int _allocate_and_init_data_node(size_t key_len, bool is_concurrency_enabled, data_node** data_node_ptr) {
    
    data_node *node = (data_node *)allocate_memory(sizeof(data_node) + key_len);
    if (node == NULL) return -10; // Handle memory allocation failure

    node->data = NULL;
    node->data_size = 0;
    node->is_concurrency_enabled = is_concurrency_enabled;

    if(is_concurrency_enabled)
    {
        if(pthread_mutex_init(&node->lock, NULL) != 0) {
            delete_data_node(node);
            return -11; // Handle mutex initialization failure
        }
    }

    *data_node_ptr = node;
    return 0;
}

/**
 * @fn _add_data_to_node
 * @brief Adds data to the specified data node.
 * Copies the provided data into the node's data buffer and sets the data size.
 * @param node_ptr Pointer to the data_node structure to which the data will be added.
 * @param value The data and its size to be copied.
 * @return int 0 on success, -1 on memory allocation failure.
 */
int _add_data_to_node(data_node *node_ptr, key_store_value* value) 
{   
    if(value->data_size == 0)
    {
        node_ptr->data = NULL;
        node_ptr->data_size = 0;
        return 0;
    }

    node_ptr->data = (unsigned char *)allocate_memory(value->data_size);

    if (node_ptr->data == NULL) {
        return -10; // Handle memory allocation failure
    }

    memcpy(node_ptr->data, value->data, value->data_size);
    node_ptr->data_size = value->data_size;

    return 0;
}


/**
 * @fn _add_key_to_node
 * @brief Adds a key to the specified data node.
 * Copies the provided key into the node's key buffer and sets the key hash.
 * @param node_ptr Pointer to the data_node structure to which the key will be added.
 * @param key The key string to be copied.
 * @param key_len Length of the key string including null terminator.
 * @param key_hash Hash value of the key to be stored in the node.
 * @return 0 on success.
 */
int _add_key_to_node(data_node *node_ptr, const char *key, size_t key_len, uint32_t key_hash) 
{
    if (node_ptr == NULL || key == NULL || key[0] == '\0' || key_len == 0) return -20; // Handle null pointer or invalid key

    memcpy(node_ptr->key, key, key_len);
    node_ptr->key[key_len - 1] = '\0';  // Ensure null termination

    node_ptr->key_hash = key_hash;

    return 0;
}

/**
 * @fn update_node_data
 * @brief Updates the data in the specified data node.
 * Reallocates memory if the new data size differs from the current size,
 * and copies the new data into the node's data buffer.
 * @param node_ptr Pointer to the data_node structure to be updated.
 * @param new_value The new data and its size to be copied.
 * @return int 0 on success, -1 on memory allocation failure.
 * @note If new_value.data_size is 0, the node's data will be set to NULL and data_size to 0.
 * @note If the new data size is the same as the current size, no reallocation is performed.
*/
int _update_data_node(data_node *node_ptr, key_store_value* new_value) {

    if(new_value == NULL || new_value->data == NULL) return -20; // Handle null pointer

    if(new_value->data_size == 0)
    {
        free_memory(node_ptr->data, NO_POOL);
        node_ptr->data = NULL;
        node_ptr->data_size = 0;
        return 0;
    }

    if(node_ptr->data_size != new_value->data_size) {
        unsigned char *new_data = (unsigned char *)reallocate_memory(node_ptr->data, new_value->data_size);
        if (new_data == NULL)  return -10; // Handle memory allocation failure

        node_ptr->data = new_data;
        node_ptr->data_size = new_value->data_size;
    }

    memcpy(node_ptr->data, new_value->data, new_value->data_size);
    return 0;
}
#pragma endregion