/**
 * @file sub_hash_table_operation.h
 * @brief Defines operations for sub hash table management in the keystore.
 * This header provides function declarations for setting, getting, and deleting
 * data nodes in a sub hash table implemented using sub hash buckets.
 * @note Only linked list based sub hash buckets are supported.
 * @note The sub hash table will be dynamically resized when the linked list chains exceed a certain threshold.
 * @note Deleted nodes are soft deleted and cleaned up during or while evaluating resizing.
**/

#ifndef SUB_BUCKET_OPERATIONS_H
#define SUB_BUCKET_OPERATIONS_H

#include "type_definitions/hash_bucket_type_definition.h"
#include "type_definitions/error_code_definitions.h"


#pragma region Type Definitions

#pragma endregion

/**
 * @fn create_sub_hash_table
 * @brief Creates and initializes a new sub-hash-table with the specified bucket size and concurrency setting.
 * @param bucket_size The number of buckets in the sub-hash-table (should be a power of two).
 * @param is_concurrency_enabled Whether to enable concurrency control for the sub-hash-table.
 * @param earlyInitialize Whether to eagerly initialize all sub-hash-buckets.
 * @param sub_hash_table_out Pointer to receive the created sub-hash-table memory pool.
 * @return int Returns 0 on success, or a negative value on failure.
 */
int create_new_sub_hash_table(sub_hash_table_configuration config, bool earlyInitialize, sub_hash_table_memory_pool** sub_hash_table_out);

/**
 * @fn cleanup_sub_hash_table
 * @brief Cleans up a sub-hash-table by deleting all its sub-hash-buckets and their linked list nodes.
 * @param sub_hash_table_ptr Pointer to the sub-hash-table to be cleaned up.
 * @return int Returns 0 on success, or a negative value on failure.
 */
int cleanup_sub_hash_table(sub_hash_table_memory_pool* sub_hash_table_ptr);

/**
 * @fn upsert_node_to_sub_hash_table
 * @brief Sets or updates a data node in the sub-hash-table by key and key hash.
 * @param sub_hash_table_ptr Pointer to the sub-hash-table memory pool.
 * @param key_hash The composite hash of the key, it contains both bucket and sub bucket key hash.
 * @param new_value Pointer to the key-value pair to be inserted or updated.
 * @return positive result on success, negative result if the node was not found or on error
 * @note When the result is 10, it indicates that a new node was created.
 * @note When the result is 0, it indicates that an existing node was updated.
 * @note When the result is 20, it indicates the caller should initiate resizing of the sub hash table.
 */
int upsert_node_to_sub_hash_table(sub_hash_table_memory_pool* sub_hash_table_ptr, composite_key_hash key_hash, key_value_pair* new_value);

/**
 * @fn get_key_store_value_from_sub_hash_table
 * @brief Retrieves a data node's value from the sub-hash-table by key and key hash.
 * @param sub_hash_table_ptr Pointer to the sub-hash-table memory pool.
 * @param key_hash The composite hash of the key, it contains both bucket and sub bucket key hash.
 * @param key The key string.
 * @param value_out Pointer to a key_value_pair structure to receive the found value.
 * @return int Returns 0 on success, or a negative value if the node was not found.
 */
int get_key_store_value_from_sub_hash_table(sub_hash_table_memory_pool* sub_hash_table_ptr, composite_key_hash key_hash, const char* key, key_value_pair* value_out);


/**
 * @fn delete_key_from_sub_hash_table
 * @brief Deletes a data node from the sub-hash-table by key and key hash.
 * @param sub_hash_table_ptr Pointer to the sub-hash-table memory pool.
 * @param key_hash The composite hash of the key, it contains both bucket and sub bucket key hash.
 * @param key The key string.
 * @return int Returns 0 on success, or a negative value if the node was not found.
 */
int delete_key_from_sub_hash_table(sub_hash_table_memory_pool* sub_hash_table_ptr, composite_key_hash key_hash, const char* key);


/**
 * @fn is_node_in_sub_hash_table
 * @brief Checks if a data node exists in the sub-hash-table by key and key hash.
 * @param sub_hash_table_ptr Pointer to the sub-hash-table memory pool.
 * @param key_hash The composite hash of the key, it contains both bucket and sub bucket key hash.
 * @param key The key string.
 * @return int Returns 0 if the node exists, or a negative value if the node was not found.
 */
int is_node_in_sub_hash_table(sub_hash_table_memory_pool* sub_hash_table_ptr, composite_key_hash key_hash, const char* key);

#endif // SUB_BUCKET_OPERATIONS_H