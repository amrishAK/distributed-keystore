#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "data_node.h"
#include "utils/memory_manager.h"

#pragma region Private Function Declarations
data_node* _allocate_and_init_data_node(size_t key_len, bool is_concurrency_enabled);
int _add_data_to_node(data_node *node_ptr, key_store_value* value);
int _add_key_to_node(data_node *node_ptr, const char *key, size_t key_len, uint32_t key_hash);
int _update_data_node(data_node *node_ptr, key_store_value* new_value);

#pragma endregion

#pragma region Public Function Definitions

data_node* create_data_node(const char *key, uint32_t key_hash, key_store_value* value, bool is_concurrency_enabled) 
{
    // Argument validation
    if (key == NULL || key[0] == '\0' || value == NULL || value->data == NULL || value->data_size == 0) return NULL;

    // Allocation and initialisation
    size_t key_len = strlen(key) + 1;
    data_node *node = _allocate_and_init_data_node(key_len, is_concurrency_enabled);
    if (node == NULL) return NULL;

    // Add key to node
    if(_add_key_to_node(node, key, key_len, key_hash) != 0) {
       delete_data_node(node);
       return NULL;
    }

    // Add data to node
    if (_add_data_to_node(node, value) != 0) {
       delete_data_node(node);
       return NULL;
    }
    
    return node;
}


int update_data_node(data_node *node_ptr, key_store_value* new_value) {

    if (node_ptr == NULL || new_value == NULL || new_value->data == NULL) return -1; // Handle null pointer
    return _update_data_node(node_ptr, new_value);
}

int delete_data_node(data_node *node_ptr) {

    if (node_ptr == NULL) return 0; // Handle null pointer, nothing to delete

    free_memory(node_ptr->data, NO_POOL);
    if(node_ptr->is_concurrency_enabled) pthread_mutex_destroy(&node_ptr->lock);
    free_memory(node_ptr, NO_POOL);

    return 0;
}

int get_data_from_node(data_node *node_ptr, key_store_value *value_out) {
    if (node_ptr == NULL || value_out == NULL) return -1; // Handle null pointer

    if (node_ptr->data_size == 0 || node_ptr->data == NULL) {
        value_out->data = NULL;
        value_out->data_size = 0;
        return 0;
    }

    value_out->data = (unsigned char *)allocate_memory(node_ptr->data_size);
    if (value_out->data == NULL) return -1; // Handle memory allocation failure

    memcpy(value_out->data, node_ptr->data, node_ptr->data_size);
    value_out->data_size = node_ptr->data_size;

    return 0;
}

#pragma endregion

#pragma region Private Function Definitions


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
data_node* _allocate_and_init_data_node(size_t key_len, bool is_concurrency_enabled) {
    data_node *node = (data_node *)allocate_memory(sizeof(data_node) + key_len);

    if (node == NULL) return NULL; // Handle memory allocation failure

    node->data = NULL;
    node->data_size = 0;
    node->is_concurrency_enabled = is_concurrency_enabled;

    if(is_concurrency_enabled)
    {
        if(pthread_mutex_init(&node->lock, NULL) != 0) {
            delete_data_node(node);
            return NULL;
        }
    }
    return node;
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
        return -1; // Handle memory allocation failure
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
    if (node_ptr == NULL || key == NULL || key[0] == '\0' || key_len == 0) return -1; // Handle null pointer or invalid key

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

    if(new_value == NULL || new_value->data == NULL) return -1; // Handle null pointer

    if(new_value->data_size == 0)
    {
        free_memory(node_ptr->data, NO_POOL);
        node_ptr->data = NULL;
        node_ptr->data_size = 0;
        return 0;
    }

    if(node_ptr->data_size != new_value->data_size) {
        unsigned char *new_data = (unsigned char *)reallocate_memory(node_ptr->data, new_value->data_size);
        if (new_data == NULL)  return -1; // Handle memory allocation failure

        node_ptr->data = new_data;
        node_ptr->data_size = new_value->data_size;
    }

    memcpy(node_ptr->data, new_value->data, new_value->data_size);
    return 0;
}
#pragma endregion