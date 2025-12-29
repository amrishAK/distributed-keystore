#include "data_node_operation.h"
#include "utils/memory_manager.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#pragma region Private Helper Function Declarations
int _allocate_and_init_data_node(size_t key_len, bool is_concurrency_enabled, data_node** data_node_ptr);
int _add_data_to_node(data_node *node_ptr, key_value_pair* key_pair);
int _add_key_to_node(data_node *node_ptr, const char *key, size_t key_len, uint32_t key_hash);
int _update_operation_counters(data_node_operation_t operation_type, int operation_result);
#pragma endregion

#pragma region Private Function Declarations
int _create_new_data_node(uint32_t key_hash, key_value_pair* kv_pair, bool is_concurrency_enabled, data_node** new_data_node_out);
int _edit_data_node_value(data_node *node_ptr, key_value_pair* new_key_pair);
int _read_data_from_node(data_node *node_ptr, key_value_pair* key_value_out);
int _delete_data_node(data_node* data_node_ptr);
int _soft_delete_data_node(data_node* data_node_ptr);
#pragma endregion


#pragma region global Variables
data_node_operation_stats g_data_node_operation_counters = {0};
#pragma endregion


#pragma region Public Function Definitions

int create_new_data_node(uint32_t key_hash, key_value_pair* kv_pair, bool is_concurrency_enabled, data_node** new_data_node_out) 
{
    int result = _create_new_data_node(key_hash, kv_pair, is_concurrency_enabled, new_data_node_out);
    return _update_operation_counters(CREATE_NODE, result);
}

int edit_data_node_value(data_node *data_node_ptr, key_value_pair* new_kv_pair) {

    int result = _edit_data_node_value(data_node_ptr, new_kv_pair);
    return _update_operation_counters(UPDATE_NODE, result);
}

int read_data_node_value(data_node* data_node_ptr, key_value_pair* kv_pair_out) {

    int result = _read_data_from_node(data_node_ptr, kv_pair_out);
    return _update_operation_counters(READ_NODE, result);
}

int delete_data_node(data_node *data_node_ptr) {

    int result = _delete_data_node(data_node_ptr);
    return _update_operation_counters(DELETE_NODE, result);
}

int soft_delete_data_node(data_node* data_node_ptr) {

    int result = _soft_delete_data_node(data_node_ptr);
    return _update_operation_counters(SOFT_DELETE_NODE, result);
}

#pragma endregion


#pragma region Private Function Definitions

int _create_new_data_node(uint32_t key_hash, key_value_pair* kv_pair, bool is_concurrency_enabled, data_node** new_data_node_out) 
{
    // Argument validation
    if (kv_pair == NULL || new_data_node_out == NULL) return ERR_INVALID_ARGUMENT; 
    if( kv_pair->key == NULL || kv_pair->key[0] == '\0' || kv_pair->value == NULL) return ERR_INVALID_ARGUMENT;

    // Allocation and initialisation
    size_t key_len = strlen(kv_pair->key) + 1;
    data_node* node = NULL;
    int alloc_result = _allocate_and_init_data_node(key_len, is_concurrency_enabled, &node);
    if (alloc_result != 0) return alloc_result;

    // Add key to node
     if(_add_key_to_node(node, kv_pair->key, key_len, key_hash) != 0) {
         delete_data_node(node);
         return ERR_DATA_NODE_CREATION_FAILED;
     }

     // Add data to node
     if (_add_data_to_node(node, kv_pair) != 0) {
         delete_data_node(node);
         return ERR_DATA_NODE_CREATION_FAILED;
     }
    
    *new_data_node_out  = node;
    return 0;
}

int _delete_data_node(data_node *data_node_ptr) {

    
    int result = 0;
    if (data_node_ptr == NULL) return 0; // Handle null pointer, nothing to delete

    free_memory(data_node_ptr->data, false);
    data_node_ptr->data = NULL;
    
    if(data_node_ptr->is_concurrency_enabled){
        result = pthread_mutex_destroy(&data_node_ptr->lock);
    }

    free_memory(data_node_ptr, false);
    return result;
}

int _soft_delete_data_node(data_node* data_node_ptr) {
    if (data_node_ptr == NULL) return ERR_INVALID_ARGUMENT; // Handle null pointer

    data_node_ptr->is_deleted = true;
    return 0;
}

int _read_data_from_node(data_node *data_node_ptr, key_value_pair* kv_pair_out) {
    if (data_node_ptr == NULL || kv_pair_out == NULL) return ERR_INVALID_ARGUMENT; // Handle null pointer
    
    if (data_node_ptr->data_size == 0 || data_node_ptr->data == NULL) {
        kv_pair_out->value = NULL;
        kv_pair_out->value_size = 0;
        return 0;
    }

    // Allocate memory for value
    kv_pair_out->value = (unsigned char *)allocate_memory(data_node_ptr->data_size);
    if (kv_pair_out->value == NULL) return ERR_MEMORY_ALLOCATION_FAILED; // Handle memory allocation failure

    // Copy value data
    memcpy(kv_pair_out->value, data_node_ptr->data, data_node_ptr->data_size);
    kv_pair_out->value_size = data_node_ptr->data_size;

    // Allocate memory for key
    size_t key_len = strlen(data_node_ptr->key) + 1;
    kv_pair_out->key = (char *)allocate_memory(key_len);
    if (kv_pair_out->key == NULL) {
        free_memory(kv_pair_out->value, false);
        return ERR_MEMORY_ALLOCATION_FAILED; // Handle memory allocation failure
    }

    // Copy key data
    memcpy((char *)kv_pair_out->key, data_node_ptr->key, key_len);
    kv_pair_out->key[key_len - 1] = '\0';  // Ensure

    return 0;
}

int _edit_data_node_value(data_node* data_node_ptr, key_value_pair* new_kv_pair) {
    
    if (data_node_ptr == NULL || new_kv_pair == NULL || new_kv_pair->value == NULL) return ERR_INVALID_ARGUMENT; // Handle null pointer
    
    if(new_kv_pair->value_size == 0)
    {
        free_memory(data_node_ptr->data, false);
        data_node_ptr->data = NULL;
        data_node_ptr->data_size = 0;
        return 0;
    }

    if(data_node_ptr->data_size != new_kv_pair->value_size) {
        unsigned char *new_data = (unsigned char *)reallocate_memory(data_node_ptr->data, new_kv_pair->value_size);
        if (new_data == NULL)  return ERR_MEMORY_ALLOCATION_FAILED; // Handle memory allocation failure

        data_node_ptr->data = new_data;
        data_node_ptr->data_size = new_kv_pair->value_size;
    }

    memcpy(data_node_ptr->data, new_kv_pair->value, new_kv_pair->value_size);
    return 0;
}


#pragma region Private Helper Function Definitions

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
 * @note Always initialise the lock first before other fields to ensure thread safety.
 */
int _allocate_and_init_data_node(size_t key_len, bool is_concurrency_enabled, data_node** data_node_ptr) {
    
    if(data_node_ptr == NULL) return ERR_INVALID_ARGUMENT;

    data_node *node = (data_node *)allocate_memory(sizeof(data_node) + key_len);
    if (node == NULL) return ERR_MEMORY_ALLOCATION_FAILED; // Handle memory allocation failure

    if(is_concurrency_enabled)
    {
        if(pthread_mutex_init(&node->lock, NULL) != 0) {
            delete_data_node(node);
            return ERR_RESOURCE_INIT_FAILED; // Handle mutex initialization failure
        }
    }

    node->data = NULL;
    node->data_size = 0;
    node->is_concurrency_enabled = is_concurrency_enabled;
    node->is_deleted = false;

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
int _add_data_to_node(data_node *node_ptr, key_value_pair* key_pair) 
{   
    if(key_pair->value_size == 0)
    {
        node_ptr->data = NULL;
        node_ptr->data_size = 0;
        return 0;
    }

    node_ptr->data = (unsigned char *)allocate_memory(key_pair->value_size);

    if (node_ptr->data == NULL) {
        return ERR_MEMORY_ALLOCATION_FAILED; // Handle memory allocation failure
    }

    memcpy(node_ptr->data, key_pair->value, key_pair->value_size);
    node_ptr->data_size = key_pair->value_size;

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
    if (node_ptr == NULL || key == NULL || key[0] == '\0' || key_len == 0) return ERR_INVALID_ARGUMENT; // Handle null pointer or invalid key

    memcpy(node_ptr->key, key, key_len);
    node_ptr->key[key_len - 1] = '\0';  // Ensure null termination

    node_ptr->key_hash = key_hash;

    return 0;
}

int _update_operation_counters(data_node_operation_t operation_type, int operation_result) 
{
    switch(operation_type) {
        case CREATE_NODE:
            if (operation_result == SUCCESS) {
                g_data_node_operation_counters.successful_create_operations++;
            } else {
                g_data_node_operation_counters.failed_create_operations++;
            }
            break;
        case DELETE_NODE:
            if (operation_result == SUCCESS) {
                g_data_node_operation_counters.successful_delete_operations++;
            } else {
                g_data_node_operation_counters.failed_delete_operations++;
            }
            break;
        case SOFT_DELETE_NODE:
            if (operation_result == SUCCESS) {
                g_data_node_operation_counters.successful_soft_delete_operations++;
            } else {
                g_data_node_operation_counters.failed_soft_delete_operations++;
            }
            break;
        case UPDATE_NODE:
            if (operation_result == SUCCESS) {
                g_data_node_operation_counters.successful_update_operations++;
            } else {
                g_data_node_operation_counters.failed_update_operations++;
            }
            break;
        case READ_NODE:
            if (operation_result == SUCCESS) {
                g_data_node_operation_counters.successful_read_operations++;
            } else {
                g_data_node_operation_counters.failed_read_operations++;
            }
            break;
        default:
            break;
    }

    if (operation_result < 0) {
        int error_index = -operation_result;
        if (error_index < 100) {
            g_data_node_operation_counters.error_code_counters[error_index]++;
        }
    }

    return operation_result;
}

#pragma endregion