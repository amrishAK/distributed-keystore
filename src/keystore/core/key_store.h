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
 * @param pre_memory_allocation_factor A factor (0 to 1) indicating the proportion of memory to pre-allocate for efficiency.
 * @param is_concurrency_enabled Flag to enable or disable concurrency control.
 * @return 0 on success, or a negative error code on failure.
 * 
 * @note The bucket_size must be a power of two. If it is not, the function returns -1 to indicate an error.
 * 
 */
int initialise_key_store(unsigned int bucket_size,  double pre_memory_allocation_factor, bool is_concurrency_enabled);

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
 * @brief Sets or updates the value associated with the specified key in the key store.
 *
 * This function adds a new key-value pair to the key store or updates the value
 * if the key already exists. The value is provided as a key_store_value structure.
 *
 * @param key The key to set or update (null-terminated string).
 * @param value Pointer to a key_store_value structure containing the data and its size.
 * @return 0 on success, or a negative error code on failure.
 * 
 * @note The caller is responsible for managing the memory of the data pointer in value.
 */
int set_key(const char *key, key_store_value* value);

/**
 * @fn get_key
 * @brief Retrieves the value associated with the specified key from the key store.
 *
 * This function looks up the given key in the key store and retrieves its associated value.
 * If the key is found, the value is copied into the provided output structure.
 *
 * @param key The key to look up (null-terminated string).
 * @param value_out Pointer to a key_store_value structure to receive the value. It is set to NULL if the key is not found. 
 * @return 0 on success, or a negative error code if the key is not found or an error occurs.
 * @note The caller is responsible for managing the memory of the data pointer in value_out.
 */
int get_key(const char *key, key_store_value* value_out);

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

/**
 * @fn get_keystore_stats
 * @brief Retrieves statistics about the key store.
 *
 * This function gathers various statistics about the key store, including
 * key distribution, memory usage, and operation counts.
 *
 * @return A keystore_stats structure containing the collected statistics.
 */
keystore_stats get_keystore_stats(void);

#endif // KEY_STORE_H