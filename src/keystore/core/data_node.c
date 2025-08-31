#include <string.h>
#include <stdlib.h>
#include "data_node.h"

// function declaration
char* string_dump(const char* s);

/**
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
data_node* create_data_node(const char *key, uint32_t key_hash, const unsigned char *data, size_t data_size) {
    
    if(data == NULL || data_size == 0) {
        return NULL; // Handle invalid data
    }

    data_node *node = (data_node *)malloc(sizeof(data_node));
    
    if (node == NULL) {
        return NULL; // Handle memory allocation failure
    }

    node->key = string_dump(key);
    
    if (node->key == NULL) {
        free(node);
        return NULL; // Handle memory allocation failure
    }

    node->key_hash = key_hash;

    node->data = (unsigned char *)malloc(data_size);
    
    if (node->data == NULL) {
        free(node->key);
        free(node);
        return NULL; // Handle memory allocation failure
    }
    memcpy(node->data, data, data_size);
    node->data_size = data_size;

    return node;
}


/**
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

    if(data == NULL || data_size == 0) {
        return -1; // Handle invalid data
    }

    unsigned char *new_data = (unsigned char *)malloc(data_size);
    
    if (new_data == NULL) {
        return -1; // Handle memory allocation failure
    }

    memcpy(new_data, data, data_size);
    free(node->data);
    node->data = new_data;
    node->data_size = data_size;

    return 0;
}


/**
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

    free(node->key);
    free(node->data);
    free(node);

    return 0;
}


/**
 * @brief Creates a duplicate of the given string.
 *
 * Allocates memory and copies the contents of the input string `s` into a newly allocated buffer.
 * The returned string must be freed by the caller using `free()`.
 *
 * @param s The input string to duplicate. If `NULL`, returns `NULL`.
 * @return A pointer to the duplicated string, or `NULL` if allocation fails or input is `NULL`.
 */
char* string_dump(const char* s) {
    if (s == NULL) return NULL;
    size_t len = strlen(s) + 1;
    char* copy = (char*)malloc(len);
    if (copy) memcpy(copy, s, len);
    return copy;
}

