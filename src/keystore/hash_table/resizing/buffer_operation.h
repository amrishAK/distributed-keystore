#ifndef BUFFER_OPERATION_H
#define BUFFER_OPERATION_H

#include "type_definitions/hash_bucket_type_definition.h"
#include "type_definitions/custom_type_definitions.h"
#include "type_definitions/error_code_definitions.h"
#include "type_definitions/sucess_code_definitions.h"


/**
 * @fn initialize_chase_worker
 * @brief Initializes a chase worker for the resizing buffer.
 * This function sets up the necessary state for a chase worker to start processing operations in the resizing buffer.
 * @param resizing_buffer_ptr Pointer to the resizing buffer for which the chase worker is being initialized.
 * @param out_task_uuid Pointer to receive the unique identifier for the initialized chase worker task.
 * @return int Returns 0 on success, or a negative value on failure.
 */
int initialize_chase_worker(resizing_buffer* resizing_buffer_ptr, uint32_t* out_task_uuid);

/**
 * @fn wait_for_chase_worker_to_finish
 * @brief Waits for a chase worker to finish processing operations in the resizing buffer.
 * This function blocks until the specified chase worker task has completed its work, ensuring that all pending operations have been processed before proceeding with resizing.
 * @param resizing_buffer_ptr Pointer to the resizing buffer being processed.
 * @param task_uuid Unique identifier of the chase worker task to wait for.
 * @return int Returns 0 on success, or a negative value on failure.
 */
int wait_for_chase_worker_to_finish(resizing_buffer* resizing_buffer_ptr, uint32_t task_uuid);

/** 
 * @fn insert_operation_to_resizing_buffer
 * @brief Inserts an operation into the resizing buffer for a hash bucket.
 * This function adds a new operation (either an update or delete) to the appropriate buffer within the resizing structure, allowing it to be processed by the chase worker during resizing.
 * @param hash_bucket_ptr Pointer to the hash bucket for which the operation is being inserted.
 * @param key_hash Composite hash value of the key associated with the operation, The composite key contains both bucket hash and sub bucket hash of the key.
 * @param key The key string associated with the operation.
 * @param kv_pair Pointer to the key-value pair for update operations (NULL for delete operations).
 * @param is_delete_operation Boolean flag indicating whether the operation is a delete (true) or an update (false).
 * @return int Returns 0 on success, or a negative value on failure.
 */
int initialize_resizing_buffer(hash_bucket* hash_bucket_ptr);

/** 
 * @fn insert_operation_to_resizing_buffer
 * @brief Inserts an operation into the resizing buffer for a hash bucket.
 * This function adds a new operation (either an update or delete) to the appropriate buffer within the resizing structure, allowing it to be processed by the chase worker during resizing.
 * @param hash_bucket_ptr Pointer to the hash bucket for which the operation is being inserted.
 * @param key_hash Composite hash value of the key associated with the operation, The composite key contains both bucket hash and sub bucket hash of the key.
 * @param key The key string associated with the operation.
 * @param kv_pair Pointer to the key-value pair for update operations (NULL for delete operations).
 * @param is_delete_operation Boolean flag indicating whether the operation is a delete (true) or an update (false).
 * @return int Returns 0 on success, or a negative value on failure.
 */
int insert_node_to_new_operation_buffer(hash_bucket* hash_bucket_ptr, composite_key_hash key_hash, key_value_pair* kv_pair, bool is_delete_operation);

/** 
 * @fn delete_resizing_buffer
 * @brief Deletes a resizing buffer and frees its associated resources.
 * This function cleans up the memory allocated for the resizing buffer, including any pending operations and associated data structures, ensuring that all resources are properly released after resizing is complete.
 * @param resizing_buffer_ptr Pointer to the resizing buffer to be deleted.
 * @return int Returns 0 on success, or a negative value on failure.
 */
int delete_resizing_buffer(resizing_buffer* resizing_buffer_ptr);

/** 
 * @fn insert_delete_operation_to_resizing_buffer
 * @brief Inserts a delete operation into the resizing buffer for a hash bucket.
 * This function adds a new delete operation to the delete operation buffer within the resizing structure, allowing it to be processed by the chase worker during resizing.
 * @param hash_bucket_ptr Pointer to the hash bucket for which the delete operation is being inserted.
 * @param key_hash Composite hash value of the key associated with the delete operation, The composite key contains both bucket hash and sub bucket hash of the key.
 * @param key The key string associated with the delete operation.
 * @return int Returns 0 on success, or a negative value on failure.
 */
int insert_delete_operation_to_resizing_buffer(hash_bucket* hash_bucket_ptr, composite_key_hash key_hash, const char* key);

/**
 * @fn insert_update_operation_to_resizing_buffer
 * @brief Inserts an update operation into the resizing buffer for a hash bucket.
 * This function adds a new update operation to the new operation buffer within the resizing structure, allowing it to be processed by the chase worker during resizing.
 * @param hash_bucket_ptr Pointer to the hash bucket for which the update operation is being inserted.
 * @param key_hash Composite hash value of the key associated with the update operation, 
 * The composite key contains both bucket hash and sub bucket hash of the key.
 * @param kv_pair Pointer to the key-value pair for the update operation.
 * @return int Returns 0 on success, or a negative value on failure.
 */
int insert_update_operation_to_resizing_buffer(hash_bucket* hash_bucket_ptr, composite_key_hash key_hash, key_value_pair* kv_pair);

/**
 * @fn get_node_from_resizing_buffer
 * @brief Retrieves a node from the resizing buffer for a hash bucket.
 * This function searches for a node in the resizing buffer, optionally ignoring the current operation buffer, and retrieves the associated key-value pair if found.
 * @param hash_bucket_ptr Pointer to the hash bucket for which the node is being retrieved.
 * @param key_hash Composite hash value of the key associated with the node, The composite key contains both bucket hash and sub bucket hash of the key.
 * @param key The key string associated with the node.
 * @param ignore_current_operation_buffer Boolean flag indicating whether to ignore the current operation buffer.
 * @param kv_pair_out Pointer to the key-value pair structure where the retrieved data will be stored.
 * @return int Returns 0 on success, or a negative value on failure.
 */
int get_node_from_resizing_buffer(hash_bucket* hash_bucket_ptr, composite_key_hash key_hash, const char* key, bool ignore_current_operation_buffer, key_value_pair* kv_pair_out);

/** 
 * @fn commit_resizing_buffer_operations_to_sub_hash_table
 * @brief Commits all pending operations in the resizing buffer to the target sub-hash-table.
 * This function processes all operations in the new operation buffer and delete operation buffer, applying them to the target sub-hash-table to ensure that all changes are reflected in the new hash bucket after resizing is complete.
 * @param hash_bucket_ptr Pointer to the hash bucket whose resizing buffer operations are being committed.
 * @return int Returns 0 on success, or a negative value on failure.
 */
int commit_resizing_buffer_operations_to_sub_hash_table(hash_bucket* hash_bucket_ptr);

#endif // BUFFER_OPERATION_H