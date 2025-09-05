#ifndef KEY_STORE_H
#define KEY_STORE_H

#include "type_definition.h"


/**
 * @fn initialise_key_store
 * @brief Initializes the key store with the specified bucket size.
 *
 * This function sets up the key store data structure, allocating resources
 * as needed to support the given number of buckets.
 *
 * @param bucket_size The number of buckets to allocate in the key store.
 * @return 0 on success, or a negative error code on failure.
 */
int initialise_key_store(int bucket_size);

/**
 * @fn cleanup_key_store
 * @brief Cleans up and releases all resources used by the key store.
 *
 * This function frees any memory and resources allocated for the key store,
 * ensuring no memory leaks occur.
 *
 * @return 0 on success, or a negative error code on failure.
 */
int cleanup_key_store(void);

/**
 * @fn set_key
 * @brief Sets a key-value pair in the key store.
 *
 * This function adds a new key-value pair to the key store, or updates the value
 * if the key already exists.
 *
 * @param key The key to be set.
 * @param data Pointer to the data to be associated with the key.
 * @param data_size Size of the data to be stored.
 * @return 0 on success, or a negative error code on failure.
 */
int set_key(const char *key, const unsigned char *data, size_t data_size);

/**
 * @fn get_key
 * @brief Retrieves the data node associated with the specified key.
 *
 * This function looks up the key in the key store and returns the corresponding
 * data node if found.
 *
 * @param key The key to look up.
 * @return Pointer to the data_node if found, or NULL if the key does not exist.
 */
data_node* get_key(const char *key);

/**
 * @fn delete_key
 * @brief Deletes a key from the key store.
 *
 * This function removes the specified key and its associated data from the key store.
 *
 * @param key The key to be deleted.
 * @return 0 on success, or a negative error code on failure.
 */
int delete_key(const char *key);

#endif // KEY_STORE_H