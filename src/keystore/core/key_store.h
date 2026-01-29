#ifndef KEY_STORE_H
#define KEY_STORE_H

#include "type_definitions/hash_bucket_type_definition.h"
#include "type_definitions/error_code_definitions.h"
#include "type_definitions/sucess_code_definitions.h"

/**
 * @fn initialise_key_store
 * @brief Initializes the key store with the specified configuration.
 *
 * This function sets up the key store with the given hash table configuration
 * and pre-allocates memory based on the provided factor.
 *
 * @param config The configuration settings for the hash table.
 * @param pre_memory_allocation_factor A factor (0.0 to 1.0) indicating the proportion of memory to pre-allocate.
 * @return 0 on success, or a negative error code on failure.
 */
int initialise_key_store(hash_table_configuration config, double pre_memory_allocation_factor);

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
int set_key(key_value_pair* value);

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
int get_key(const char *key, key_value_pair* value_out);

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