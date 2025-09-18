/**
 * @file data_node.h
 * @brief Defines the interface for managing data nodes in the distributed keystore.
 *
 * This header provides functions for creating, updating, and deleting data nodes,
 * which are used to store key-value pairs in the keystore.
 */
#ifndef DATA_NODE_H
#define DATA_NODE_H

#include <stdint.h>
#include "type_definition.h"

/**
 * @fn create_data_node
 * @brief Creates a new data node with the specified key, key hash, and value.
 *
 * @param key The key associated with the data node.
 * @param key_hash The hash value of the key.
 * @param value Pointer to the key_store_value to be stored.
 * @param is_concurrency_enabled Whether to initialize the node for concurrency.
 * @return Pointer to the newly created data_node, or NULL on failure.
 */
data_node* create_data_node(const char *key, uint32_t key_hash, key_store_value* value, bool is_concurrency_enabled);

/**
 * @fn update_data_node
 * @brief Updates the data stored in a data node.
 *
 * This function replaces the existing data in the node with new data provided
 * in the key_store_value structure. It handles memory allocation and resizing
 * as necessary.
 *
 * @param node Pointer to the data_node to be updated.
 * @param new_value Pointer to the key_store_value containing the new data.
 * @return 0 on success, non-zero on failure.
 */
int update_data_node(data_node *node, key_store_value* new_value);

/**
 * @fn get_data_from_node
 * @brief Retrieves the data stored in a data node.
 *
 * This function copies the data from the specified data node into the provided
 * key_store_value structure. The caller is responsible for managing the memory
 * of the data pointer in value_out.
 *
 * @param node Pointer to the data_node from which to retrieve data.
 * @param value_out Pointer to a key_store_value structure to receive the data.
 * @return 0 on success, non-zero on failure.
 */
int get_data_from_node(data_node *node, key_store_value* value_out);

/**
 * @fn delete_data_node
 * @brief Deletes a data node and frees its associated resources.
 *
 * This function releases all memory allocated for the data node, including
 * its data buffer and any concurrency control structures if enabled.
 *
 * @param node Pointer to the data_node to be deleted.
 * @return 0 on success, non-zero on failure.
 */
int delete_data_node(data_node *node);

#endif // DATA_NODE_H