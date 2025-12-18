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

#pragma region Type Definitions

typedef enum {
    DATA_NODE_UPDATE,
    DATA_NODE_READ,
    DATA_NODE_DELETE,
    DATA_NODE_CREATE
} data_node_operation_type_t;

#pragma endregion

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
int create_data_node(const char *key, uint32_t key_hash, key_store_value* value, bool is_concurrency_enabled, data_node** data_node_ptr);

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

/**
 * @fn data_node_mutex_lock_wrapper
 * @brief Wraps data node operations with mutex lock for concurrency control.
 *
 * This function acquires the mutex lock of the data node, performs the
 * operation (DATA_NODE_READ or DATA_NODE_UPDATE), and then releases the lock.
 *
 * @param operation_type The type of operation to perform (DATA_NODE_READ or DATA_NODE_UPDATE).
 * @param data_node_ptr Pointer to the data node on which to perform the operation.
 * @param value Pointer to a key_store_value structure for input/output as needed.
 * @return int Returns the result of the operation, or -1 on lock acquisition failure.
 */
int data_node_mutex_lock_wrapper(data_node_operation_type_t operation_type, data_node* data_node_ptr, key_store_value* value);

/**
 * @fn get_data_node_operation_counters
 * @brief Retrieves the global data node operation counters.
 *
 * This function returns a copy of the global data_node_operation_counters structure,
 * which contains counts of total and failed operations as well as error code occurrences.
 *
 * @return data_node_operation_counters A copy of the global data node operation counters.
 */
data_node_operation_counters get_data_node_operation_counters(void);

#endif // DATA_NODE_H