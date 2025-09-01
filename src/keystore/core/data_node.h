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
  * @brief Creates a new data node with the specified key, key hash, and data.
  *
  * @param key The key associated with the data node.
  * @param key_hash The hash value of the key.
  * @param data Pointer to the data to be stored.
    * @param data_size Size of the data in bytes.
    * @return Pointer to the newly created data_node, or NULL on failure.
*/
data_node* create_data_node(const char *key, uint32_t key_hash, const unsigned char *data, size_t data_size);

/**
 * @fn update_data_node
 * @brief Updates the data stored in an existing data node.
 *
 * @param node Pointer to the data_node to update.
 * @param data Pointer to the new data.
 * @param data_size Size of the new data in bytes.
 * @return 0 on success, non-zero on failure.
*/
int update_data_node(data_node *node, const unsigned char *data, size_t data_size);

/**
 * @fn delete_data_node
 * @brief Deletes a data node and frees associated resources.
 *
 * @param node Pointer to the data_node to delete.
 * @return 0 on success, non-zero on failure.
 */
int delete_data_node(data_node *node);

#endif // DATA_NODE_H