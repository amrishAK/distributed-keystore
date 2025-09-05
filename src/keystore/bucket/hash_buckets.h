#ifndef HASH_BUCKETS_H
#define HASH_BUCKETS_H

#include "core/type_definition.h"
#include <stdbool.h>



/**
 * @fn get_hash_bucket
 * @brief Retrieves a hash bucket at the specified index, optionally creating it if missing.
 * @param hash_bucket_ptr Pointer to the parent hash bucket container.
 * @param index Index of the bucket to retrieve.
 * @param create_if_missing If true, creates the bucket if it does not exist.
 * @return Pointer to the hash bucket at the specified index, or NULL if not found and not created.
 */
hash_bucket* get_hash_bucket(hash_bucket *hash_bucket_ptr, int index, bool create_if_missing);


/**
 * @fn delete_hash_bucket
 * @brief Deletes a hash bucket and frees its associated resources.
 * @param hash_bucket_ptr Pointer to the hash bucket to delete.
 * @return void
 */
void delete_hash_bucket(hash_bucket *hash_bucket_ptr);


/**
 * @fn add_node_to_bucket
 * @brief Adds a data node to the specified hash bucket.
 * @param hash_bucket_ptr Pointer to the hash bucket.
 * @param key_hash Hash value of the key associated with the node.
 * @param node_ptr Pointer to the data node to add.
 * @return 0 on success, non-zero on failure.
 */
int add_node_to_bucket(hash_bucket *hash_bucket_ptr, uint32_t key_hash, data_node *node_ptr);

/**
 * @fn find_node_in_bucket
 * @brief Finds a data node in the hash bucket by key and key hash.
 * @param hash_bucket_ptr Pointer to the hash bucket.
 * @param key Key string to search for.
 * @param key_hash Hash value of the key.
 * @return Pointer to the found data node, or NULL if not found.
 */
data_node* find_node_in_bucket(hash_bucket *hash_bucket_ptr, const char *key, uint32_t key_hash);

/**
 * @fn delete_node_from_bucket
 * @brief Deletes a data node from the hash bucket by key and key hash.
 * @param hash_bucket_ptr Pointer to the hash bucket.
 * @param key Key string of the node to delete.
 * @param key_hash Hash value of the key.
 * @return 0 on success, non-zero on failure.
 */
int delete_node_from_bucket(hash_bucket *hash_bucket_ptr, const char *key, uint32_t key_hash);


#endif // HASH_BUCKETS_H