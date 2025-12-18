#ifndef HASH_BUCKETS_H
#define HASH_BUCKETS_H

#include "core/type_definition.h"
#include <stdbool.h>

/**
 * @fn initialise_hash_buckets
 * @brief Initializes the hash bucket system with the specified bucket size.
 * @param bucket_size The number of buckets to allocate.
 * @param is_concurrency_enabled Flag to enable or disable concurrency control.
 * @return 0 on success, non-zero on failure.
 */
int initialise_hash_buckets(unsigned int bucket_size, bool is_concurrency_enabled);

/**
 * @fn cleanup_hash_buckets
 * @brief Cleans up and releases all resources used by the hash bucket system.
 * @return 0 on success, non-zero on failure.
 */
int cleanup_hash_buckets(void);

/**
 * @fn get_hash_bucket
 * @brief Retrieves a hash bucket at the specified index, optionally creating it if missing.
 * @param hash_bucket_ptr Pointer to the parent hash bucket container.
 * @param index Index of the bucket to retrieve.
 * @param create_if_missing If true, creates the bucket if it does not exist.
 * @return Pointer to the hash bucket at the specified index, or NULL if not found and not created.
 */
hash_bucket* get_hash_bucket(unsigned int index);

/**
 * @fn upsert_node_to_bucket
 * @brief Sets or updates a data node in the hash bucket by key and key hash.
 * @param index Index of the hash bucket.
 * @param key Key string of the node to set.
 * @param key_hash Hash value of the key.
 * @param new_value New value to set in the node.
 * @return 0 on success, negative result if the node was not found or on error.
 * @note This function checks if the key contains a data node.If the node does not exist, it will be created.
 */
int upsert_node_to_bucket(unsigned int index, const char *key, uint32_t key_hash, key_store_value* new_value);

/**
 * @fn find_node_in_bucket
 * @brief Finds a data node in the hash bucket by key and key hash.
 * @param index Index of the hash bucket.
 * @param key Key string to search for.
 * @param key_hash Hash value of the key.
 * @return Pointer to the found data node, or NULL if not found.
 */
int find_node_in_bucket(unsigned int index, const char *key, uint32_t key_hash, key_store_value* value_out);

/**
 * @fn delete_node_from_bucket
 * @brief Deletes a data node from the hash bucket by key and key hash.
 * @param index Index of the hash bucket.
 * @param key Key string of the node to delete.
 * @param key_hash Hash value of the key.
 * @return 0 on success, -1 if the node was not found or on error.
 */
int delete_node_from_bucket(unsigned int index, const char *key, uint32_t key_hash);

/**
 * @fn get_hash_bucket_pool_stats
 * @brief Retrieves statistics about the hash bucket memory pool.
 * @param pool_out Pointer to a keystore_stats structure to receive the statistics.
 */
void get_hash_bucket_pool_stats(keystore_stats* pool_out);

#endif // HASH_BUCKETS_H