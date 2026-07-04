#ifndef SUB_HASH_BUCKET_OPERATION_H
#define SUB_HASH_BUCKET_OPERATION_H

#include "type_definitions/hash_bucket_type_definition.h"
#include "type_definitions/custom_type_definitions.h"
#include "type_definitions/error_code_definitions.h"

/**
 * @fn initialise_sub_hash_bucket
 * @brief Initializes a sub-hash-bucket to its default state.
 *
 * This function sets up the sub-hash-bucket's linked list head, active node count,
 * concurrency settings, and initializes the read-write lock if concurrency is enabled.
 *
 * @param sub_hash_bucket_ptr Pointer to the sub-hash-bucket to be initialized.
 * @param is_concurrency_enabled Whether to enable concurrency control for the sub-hash-bucket.
 * @param max_linked_list_chain_length Maximum allowed length of linked list chains in the bucket.
 * @return int Returns 0 on success, or a negative value on failure.
 */
int initialise_sub_hash_bucket(sub_hash_bucket *sub_hash_bucket_ptr, bool is_concurrency_enabled, unsigned int max_linked_list_chain_length);

/**
 * @fn cleanup_sub_hash_bucket
 * @brief Cleans up a sub-hash-bucket by deleting all its linked list nodes
 * @param sub_hash_bucket_ptr Pointer to the sub-hash-bucket to be cleaned up.
 * @return int Returns 0 on success, or a negative value on failure.
 */
int cleanup_sub_hash_bucket(sub_hash_bucket* sub_hash_bucket_ptr);

/**
 * @fn update_node_in_sub_hash_bucket
 * @brief Sets or updates a data node in the sub-hash-bucket
 * @param args Structure containing sub-hash-bucket pointer, key, and composite key hash.
 * @param new_value New value to set in the node.
 * @return positive result on success, negative result if the node was not found or on error
 * @note When the result is 0, it indicates that an existing node was updated.
 */
int update_node_in_sub_hash_bucket(sub_hash_bucket_operation_args args, key_value_pair* new_value);

/**
 * @fn add_node_to_sub_hash_bucket
 * @brief Adds a new data node in the sub-hash-bucket by key and composite key hash.
 * @param args Structure containing sub-hash-bucket pointer, key, and composite key hash.
 * @param new_value New value to set in the node.
 * @return positive result on success, negative result on error.
 * @note When the result is 20, it indicates the caller should initiate resizing of the sub-hash-bucket.
 */
int add_node_to_sub_hash_bucket(sub_hash_bucket_operation_args args, key_value_pair* new_value);


/**
 * @fn get_key_store_value_from_sub_hash_bucket
 * @brief Retrieves a data node's value from the sub-hash-bucket by key and composite key hash.
 * @param args Structure containing sub-hash-bucket pointer, key, and composite key hash.
 * @param value_out Pointer to a key_value_pair structure to receive the found value.
 * @return int Returns 0 on success, or a negative value if the node was not found.
 */
int get_key_store_value_from_sub_hash_bucket(sub_hash_bucket_operation_args args, key_value_pair* value_out);


/**
 * @fn delete_key_from_sub_hash_bucket
 * @brief Soft deletes a data node from the sub-hash-bucket by key and composite key hash.
 * @param args Structure containing sub-hash-bucket pointer, key, and composite key hash.
 * @return int Returns 0 on successful deletion, or a negative value if the node was not found.
 * @note This function performs a soft delete, meaning the node is marked as deleted but not physically removed.
 * Later, a cleanup process is performed during resizing to free up memory.
 */
int delete_key_from_sub_hash_bucket(sub_hash_bucket_operation_args args);

/**
 * @fn is_node_in_sub_hash_bucket
 * @brief Checks if a data node exists in the sub-hash-bucket by key and composite key hash.
 * @param args Structure containing sub-hash-bucket pointer, key, and composite key hash.
 * @return int Returns 0 if the node exists, or a negative value if the node was not found.
 */
int is_node_in_sub_hash_bucket(sub_hash_bucket_operation_args args);


#endif // SUB_HASH_BUCKET_OPERATION_H