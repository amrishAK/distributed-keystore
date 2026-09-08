#ifndef DATA_NODE_OPERATION_H
#define DATA_NODE_OPERATION_H

#include "type_definitions/hash_bucket_type_definition.h"
#include "type_definitions/error_code_definitions.h"
#include "type_definitions/sucess_code_definitions.h"

typedef enum {
	CREATE_NODE,
	DELETE_NODE,
	SOFT_DELETE_NODE,
	UPDATE_NODE,
	READ_NODE
} data_node_operation_t;

/**
 * @fn create_data_node
 * @brief Creates a new data node with the specified key hash and key-value pair.
 *
 * This function allocates memory for a new data_node, initializes it with the provided
 * key hash, key, and value, and sets up concurrency control if enabled.
 *
 * @param key_hash Composite hash value of the key to be stored in the node, The composite key contains both bucket hash and sub bucket hash of the key.
 * @param kv_pair Pointer to the key_value_pair containing the key and value data.
 * @param is_concurrency_enabled Boolean flag indicating if concurrency control should be enabled.
 * @param new_data_node_out Pointer to receive the newly created data_node.
 * @return int Returns 0 on success, or a negative value on failure.
 */
int create_new_data_node(composite_key_hash key_hash, key_value_pair* kv_pair, bool is_concurrency_enabled, data_node** new_data_node_out);

/**
 * @fn delete_data_node
 * @brief Deletes a data node and frees its associated memory.
 *
 * This function releases the memory allocated for the data node's key and value,
 * destroys the mutex if concurrency control is enabled, and frees the data_node itself.
 *
 * @param data_node_ptr Pointer to the data_node to be deleted.
 */
int delete_data_node(data_node* data_node_ptr);


/**
 * @fn soft_delete_data_node
 * @brief Marks a data node as deleted without freeing its memory.
 *
 * This function sets the is_deleted flag of the data node to true,
 * indicating that it has been soft deleted. The actual memory cleanup
 * will be handled later during a cleanup process.
 *
 * @param data_node_ptr Pointer to the data_node to be soft deleted.
 * @return int Returns 0 on success, or a negative value on failure.
 */
int soft_delete_data_node(data_node* data_node_ptr);

/**
 * @fn edit_data_node_value
 * @brief Updates the value of an existing data node.
 *
 * This function replaces the current value of the data node with a new value
 * provided in the key_value_pair structure. It handles memory allocation for
 * the new value and frees the old value.
 *
 * @param data_node_ptr Pointer to the data_node to be updated.
 * @param new_kv_pair Pointer to the key_value_pair containing the new value data.
 * @return int Returns 0 on success, or a negative value on failure.
 */
int edit_data_node_value(data_node* data_node_ptr, key_value_pair* new_kv_pair);

/**
 * @fn read_data_node_value
 * @brief Reads the value from a data node into a key_value_pair structure.
 *
 * This function copies the value stored in the data node into the provided
 * key_value_pair structure, allocating memory for the value as needed.
 *
 * @param data_node_ptr Pointer to the data_node to read from.
 * @param kv_pair_out Pointer to the key_value_pair structure to receive the value.
 * @return int Returns 0 on success, or a negative value on failure.
 */
int read_data_node_value(data_node* data_node_ptr, key_value_pair* kv_pair_out);

#endif // DATA_NODE_OPERATION_H