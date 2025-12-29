#ifndef HASH_TABLE_OPERATION_H
#define HASH_TABLE_OPERATION_H

#include "type_definitions/hash_bucket_type_definition.h"

/**
 * @fn create_new_hash_table
 * @brief Creates and initializes a new hash table with the specified configuration.
 * @param config Configuration parameters for the hash table.
 * @param hash_table_out Pointer to receive the created hash table memory pool.
 * @return int Returns 0 on success, or a negative value on failure.
 */
int create_new_hash_table(hash_table_configuration config, hash_table_memory_pool** hash_table_out);

/**
 * @fn cleanup_hash_table
 * @brief Cleans up the hash table by deleting all its hash buckets and their linked list nodes.
 * @param hash_table_ptr Pointer to the hash table to be cleaned up.
 * @return int Returns 0 on success, or a negative value on failure.
 */
int cleanup_hash_table(hash_table_memory_pool* hash_table_ptr);

/**
 * @fn upsert_node_to_hash_table
 * @brief Sets or updates a data node in the hash table by key and key hash.
 * @param hash_table_ptr Pointer to the hash table memory pool.
 * @param key_hash Hash of the key.
 * @param kv_pair Pointer to the key-value pair to be inserted or updated.
 * @return positive result on success, negative result if the node was not found or on error
 * @note When the result is 10, it indicates that a new node was created.
 * @note When the result is 0, it indicates that an existing node was updated.
 * @note When the result is 20, it indicates the caller should initiate resizing of the sub hash table.
 */
int upsert_node_to_hash_table(hash_table_memory_pool* hash_table_ptr, uint32_t key_hash, key_value_pair* kv_pair);

/**
 * @fn get_key_value_from_hash_table
 * @brief Retrieves a data node's value from the hash table by key and key hash.
 * @param hash_table_ptr Pointer to the hash table memory pool.
 * @param key_hash Hash of the key.
 * @param key The key string.
 * @param kv_pair_out Pointer to a key_value_pair structure to receive the found value.
 * @return int Returns 0 on success, or a negative value if the node was not found.
 */
int get_key_value_from_hash_table(hash_table_memory_pool* hash_table_ptr, uint32_t key_hash, const char* key, key_value_pair* kv_pair_out);

/**
 * @fn delete_key_from_hash_table
 * @brief Deletes a data node from the hash table by key and key hash.
 * @param hash_table_ptr Pointer to the hash table memory pool.
 * @param key_hash Hash of the key.
 * @param key The key string.
 * @return int Returns 0 on success, or a negative value if the node was not found.
 */
int delete_key_from_hash_table(hash_table_memory_pool* hash_table_ptr, uint32_t key_hash, const char* key);

#endif // HASH_TABLE_OPERATION_H