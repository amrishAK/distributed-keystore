#ifndef DATA_HANDLER_H
#define DATA_HANDLER_H

#include "type_definitions/custom_type_definitions.h"
#include "type_definitions/error_code_definitions.h"
#include "type_definitions/sucess_code_definitions.h"

/**
 * @fn create_data_node_from_value
 * @brief Creates a new data node with the specified key, value, and concurrency settings.
 * @param key_hash The composite key hash for the data node.
 * @param key The key string for the data node.
 * @param value The value bytes for the data node.
 * @param value_size The size of the value in bytes.
 * @param is_concurrency_enabled Flag indicating whether concurrency control is enabled for this node.
 * @param new_node_out Pointer to a data_node pointer to receive the created node.
 * @return int Returns 0 on success, or a negative value if an error occurs.
 */
int create_data_node_from_value(const composite_key_hash key_hash, const char* key, const unsigned char* value, const size_t value_size, const bool is_concurrency_enabled, data_node** new_node_out);

/**
 * @fn get_data_node_value
 * @brief Retrieves the value from a data node.
 * @param data_node_ptr Pointer to the data node to read from.
 * @param is_concurrency_enabled Flag indicating whether concurrency control is enabled for this node.
 * @param value_out Pointer to a key_value_pair structure to receive the key and value.
 * @return int Returns 0 on success, or a negative value if an error occurs.
 */
int get_data_node_value(const data_node* data_node_ptr, const bool is_concurrency_enabled, key_value_pair* value_out);

/**
 * @fn update_data_node
 * @brief Updates the value of a data node.
 * @param data_node_ptr Pointer to the data node to update.
 * @param is_concurrency_enabled Flag indicating whether concurrency control is enabled for this node.
 * @param new_value Pointer to a key_value_pair structure containing the new value.
 * @return int Returns 0 on success, or a negative value if an error occurs.
 */
int update_data_node(data_node* data_node_ptr, const bool is_concurrency_enabled , const key_value_pair* new_value);

/**
 * @fn soft_delete
 * @brief Soft deletes a data node by marking it as deleted without freeing memory.
 * @param data_node_ptr Pointer to the data node to soft delete.
 * @param is_concurrency_enabled Flag indicating whether concurrency control is enabled for this node.
 * @return int Returns 0 on success, or a negative value if an error occurs.
 */
int soft_delete(data_node* data_node_ptr, const bool is_concurrency_enabled);

#endif // DATA_HANDLER_H
