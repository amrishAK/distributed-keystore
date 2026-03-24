#ifndef HASH_BUCKET_OPERATION_H
#define HASH_BUCKET_OPERATION_H

#include "type_definitions/hash_bucket_type_definition.h"

/**
 * @fn initialise_hash_bucket
 * @brief Initializes a hash bucket with the specified sub-hash-table configuration.
 * @param hash_bucket_ptr Pointer to the hash bucket to be initialized.
 * @param sub_hash_table_config Configuration parameters for the sub-hash-table.
 * @return int Returns 0 on success, or a negative value on failure.
 */
int initialise_hash_bucket(hash_bucket* hash_bucket_ptr, sub_hash_table_configuration sub_hash_table_config);

/**
 * @fn cleanup_hash_bucket
 * @brief Cleans up a hash bucket by deleting its sub-hash-table and any associated resources.
 * @param hash_bucket_ptr Pointer to the hash bucket to be cleaned up.
 * @return int Returns 0 on success, or a negative value on failure.
 */
int cleanup_hash_bucket(hash_bucket* hash_bucket_ptr);

/**
 * @fn upsert_node_to_hash_bucket
 * @brief Sets or updates a data node in the hash bucket by key hash and key-value pair.
 * @param hash_bucket_ptr Pointer to the hash bucket.
 * @param key_hash Composite hash of the key.
 * @param kv_pair Pointer to the key-value pair to be inserted or updated.
 * @return int Returns 0 on success, or a negative value on failure.
 * @note When the result is 10, it indicates that a new node was created.
 * @note When the result is 0, it indicates that an existing node was updated.
 * @note When the result is 20, it indicates resizing of the sub hash table is triggered.
 */
int upsert_node_to_hash_bucket(hash_bucket* hash_bucket_ptr, composite_key_hash key_hash, key_value_pair* kv_pair);

/**
 * @fn get_key_value_from_hash_bucket
 * @brief Retrieves a data node's value from the hash bucket by key and key hash.
 * @param hash_bucket_ptr Pointer to the hash bucket.
 * @param key The key string.
 * @param key_hash Composite hash of the key.
 * @param kv_pair_out Pointer to a key_value_pair structure to receive the found value.
 * @return int Returns 0 on success, or a negative value if the node was not found.
 */
int get_key_value_from_hash_bucket(hash_bucket* hash_bucket_ptr, const char *key, composite_key_hash key_hash, key_value_pair* kv_pair_out);

/**
 * @fn delete_key_from_hash_bucket
 * @brief Deletes a data node from the hash bucket by key and key hash.
 * This function only soft deletes the node by marking it as deleted, 
 * allowing for deferred cleanup while checking for resizing condition or while resizing to optimize performance.
 * @param hash_bucket_ptr Pointer to the hash bucket.
 * @param key The key string.
 * @param key_hash Composite hash of the key.
 * @return int Returns 0 on success, or a negative value if the node was not found.
 * @note When the result is 0, it indicates that the node was successfully marked as deleted.
 */
int delete_key_from_hash_bucket(hash_bucket* hash_bucket_ptr, const char *key, composite_key_hash key_hash);

#endif // HASH_BUCKET_OPERATION_H