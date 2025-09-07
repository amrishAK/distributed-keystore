#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "data_node.h"
#include "utils/memory_manager.h"

// function declaration
int add_data_to_node(data_node *node, const unsigned char *data, size_t data_size);
int add_key_to_node(data_node *node, const char *key, size_t key_len, uint32_t key_hash);
int update_node_data(data_node *node_ptr, const unsigned char *data, size_t data_size);

/**
 * @fn create_data_node
 * @brief Creates a new data_node instance with the specified key, key hash, and data.
 *
 * Allocates memory for a data_node structure, copies the provided key and data,
 * and initializes the key hash and data size fields. Handles memory allocation failures
 * by freeing any allocated resources and returning NULL.
 *
 * @param key        The key string to associate with the data node.
 * @param key_hash   The hash value of the key.
 * @param data       Pointer to the data to be stored in the node.
 * @param data_size  Size of the data in bytes.
 *
 * @return Pointer to the newly created data_node, or NULL if memory allocation fails.
 */
data_node* create_data_node(const char *key, uint32_t key_hash, const unsigned char *data, size_t data_size) 
{
    // Argument validation
    if (key == NULL || key[0] == '\0' || data == NULL || data_size == 0) {
        return NULL; // Invalid key or data
    }

    size_t key_len = strlen(key) + 1;
    data_node *node = (data_node *)allocate_memory(sizeof(data_node) + key_len);
    
    if (node == NULL) {
        return NULL; // Handle memory allocation failure
    }

    if(add_key_to_node(node, key, key_len, key_hash) != 0) {
       delete_data_node(node);
       return NULL;
    }

   if (add_data_to_node(node, data, data_size) != 0) {
       delete_data_node(node);
       return NULL;
    }

    return node;
}


/**
 * @fn update_data_node
 * @brief Updates the data stored in a data_node structure.
 *
 * This function replaces the current data in the given data_node with new data provided
 * by the caller. It allocates memory for the new data, copies the contents, frees the
 * previously allocated memory, and updates the data size.
 *
 * @param node Pointer to the data_node to be updated.
 * @param data Pointer to the new data to be stored.
 * @param data_size Size of the new data in bytes.
 * @return 0 on success, -1 if node is NULL or memory allocation fails.
 */
int update_data_node(data_node *node, const unsigned char *data, size_t data_size) {
    
    if (node == NULL) {
        return -1; // Handle null pointer
    }

    if(data == NULL) {
        return -1; // Handle invalid data
    }

    int result = update_node_data(node, data, data_size);
    if (result != 0)
    {
        return -1; // Handle memory allocation failure
    }
    
    node->data_size = data_size;

    return 0;
}


/**
 * @fn delete_data_node
 * @brief Deletes a data_node and frees its associated memory.
 *
 * This function releases the memory allocated for the key and data fields
 * of the given data_node, as well as the memory for the node itself.
 *
 * @param node Pointer to the data_node to be deleted.
 * @return int Returns 0 on success, or -1 if the node pointer is NULL.
 */
int delete_data_node(data_node *node) {
    
    if (node == NULL) {
        return -1; // Handle null pointer
    }

    free_memory(node->data, NO_POOL);
    free_memory(node, NO_POOL);

    return 0;
}


/**
 * @fn add_data_to_node
 * @brief Adds data to a data_node structure.
 *
 * Allocates memory for the data and copies the provided data into the node.
 * If the data size is zero, sets the node's data pointer to NULL and data size to 0.
 * Returns 0 on success, or -1 if memory allocation fails.
 *
 * @param node Pointer to the data_node structure to which data will be added.
 * @param data Pointer to the data to be copied into the node.
 * @param data_size Size of the data to be copied.
 * @return int 0 on success, -1 on memory allocation failure.
 */
int add_data_to_node(data_node *node, const unsigned char *data, size_t data_size) 
{   
    if(data_size == 0)
    {
        node->data = NULL;
        node->data_size = 0;
        return 0;
    }

    node->data = (unsigned char *)allocate_memory(data_size);
    
    if (node->data == NULL) {
        return -1; // Handle memory allocation failure
    }

    memcpy(node->data, data, data_size);
    node->data_size = data_size;

    return 0;
}


/**
 * @fn add_data_to_node
 * @brief Adds data to the specified data node.
 *
 * Copies the provided key into the node's key buffer, ensures null termination,
 * and sets the node's key hash value.
 *
 * @param node Pointer to the data_node structure to which the key will be added.
 * @param key Pointer to the key data to be copied.
 * @param key_len Length of the key data, including space for null termination.
 * @param key_hash Hash value of the key to be stored in the node.
 * @return 0 on success.
 */
int add_key_to_node(data_node *node, const char *key, size_t key_len, uint32_t key_hash) 
{
    memcpy(node->key, key, key_len);
    node->key[key_len - 1] = '\0';  // Ensure null termination

    node->key_hash = key_hash;

    return 0;
}

/**
 * @fn add_data_to_node
 * @brief Adds data to a data_node structure.
 *
 * This function replaces the contents of the node's data buffer with the provided data.
 * If the new data size is zero, the node's data buffer is freed and set to NULL.
 * If the new data size differs from the current size, the buffer is reallocated.
 * The function handles memory allocation failures gracefully.
 *
 * @param node_ptr Pointer to the data_node to be updated.
 * @param data Pointer to the new data to store in the node.
 * @param data_size Size of the new data in bytes.
 * @return 0 on success, -1 if memory allocation fails.
 */
int update_node_data(data_node *node_ptr, const unsigned char *data, size_t data_size) {

    if(data_size == 0)
    {
        free(node_ptr->data);
        node_ptr->data = NULL;
        node_ptr->data_size = 0;
        return 0;
    }

    if(node_ptr->data_size != data_size) {
        unsigned char *new_data = (unsigned char *)reallocate_memory(node_ptr->data, data_size);
        if (new_data == NULL) {
            return -1; // Handle memory allocation failure
        }

        node_ptr->data = new_data;
        node_ptr->data_size = data_size;
    }

    memcpy(node_ptr->data, data, data_size);
    return 0;
}
