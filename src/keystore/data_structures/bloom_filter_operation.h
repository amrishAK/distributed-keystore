/**
 * @file bloom_filter_operation.h
 * @brief Header file for Bloom filter operations in the distributed keystore.
 * This header defines the interface for initializing, cleaning up, adding keys to, and checking keys in a Bloom filter. The Bloom filter is used to efficiently determine the potential presence of keys in sub-buckets during resizing operations, helping to optimize lookups and reduce unnecessary disk accesses.
 * The functions declared in this header allow for the management of the Bloom filter's lifecycle and its interaction with keys, providing a probabilistic mechanism to enhance the performance of the keystore.
 * Functions:
 * - init_bloom_filter: Initializes a Bloom filter based on the maximum chain length of a sub-bucket.
 * - cleanup_bloom_filter: Cleans up resources associated with a Bloom filter.
 * - add_key_to_bloom_filter: Adds a key to the Bloom filter, updating the bit array accordingly.
 * - check_key_in_bloom_filter: Checks if a key is likely present in the Bloom filter, returning a boolean result.
 * Note: The Bloom filter provides a probabilistic result, meaning that it may indicate that a key is present when it is not (false positive), but it will never indicate that a key is absent if it is indeed present (no false negatives).
 */
#ifndef BLOOM_FILTER_OPERATION_H
#define BLOOM_FILTER_OPERATION_H

#include "type_definitions/custom_type_definitions.h"

/**
 * @fn initialize_bloom_filter
 * @brief Initializes a Bloom filter based on the maximum chain length of a sub-bucket.
 * This function calculates the optimal parameters for the Bloom filter, including the number of hash functions and the size of the bit array, based on the provided maximum chain length. It allocates memory for the bit array and initializes it to zero.
 * @param max_chain_length The maximum number of entries expected in a sub-bucket, used to determine the size of the Bloom filter.
 * @param bloom_filter_out Pointer to a bloom_filter_t structure where the initialized Bloom filter will be stored.
 * @return Returns 0 on success, or a non-zero error code on failure (e.g., memory allocation failure).
 */
int initialize_bloom_filter(uint32_t max_chain_length, bloom_filter_t** bloom_filter_out);

/**
 * @fn cleanup_bloom_filter
 * @brief Cleans up resources associated with a Bloom filter.
 * This function frees the memory allocated for the Bloom filter's bit array and resets its parameters. It should be called when the Bloom filter is no longer needed to prevent memory leaks.
 * @param bloom_filter Pointer to the bloom_filter_t structure representing the Bloom filter to be cleaned up.
 * @return Returns 0 on success, or a non-zero error code on failure (e.g., if the bloom_filter pointer is NULL).
 */
int cleanup_bloom_filter(bloom_filter_t* bloom_filter);

/**
 * @fn add_key_to_bloom_filter
 * @brief Adds a key to the Bloom filter.
 * This function updates the Bloom filter's bit array based on the provided key, using the filter's hash functions.
 * @param key_hash The hash of the key to be added to the Bloom filter.
 * @param bloom_filter Pointer to the bloom_filter_t structure representing the Bloom filter.
 * @return Returns 0 on success, or a non-zero error code on failure (e.g., if the bloom_filter pointer is NULL).
 */
int add_key_to_bloom_filter(composite_key_hash key_hash, bloom_filter_t* bloom_filter);

/**
 * @fn check_key_in_bloom_filter
 * @brief Checks if a key is likely present in the Bloom filter.
 * This function uses the Bloom filter's hash functions to check if the specified key is likely present. It provides a probabilistic result, meaning that it may indicate that a key is present when it is not (false positive), but it will never indicate that a key is absent if it is indeed present (no false negatives).
 * @param key_hash The hash of the key to be checked in the Bloom filter.
 * @param bloom_filter Pointer to the bloom_filter_t structure representing the Bloom filter.
 * @return Returns BLOOM_FILTER_CHECK_KEY_MAY_EXIST (31) if the key is likely present, BLOOM_FILTER_CHECK_KEY_NOT_EXIST (32) if the key is definitely not present, or a non-zero error code on failure (e.g., if the bloom_filter pointer is NULL).
 */
int check_key_in_bloom_filter(composite_key_hash key_hash, bloom_filter_t* bloom_filter);

/**
 * @fn reset_bloom_filter
 * @brief Resets the Bloom filter.
 * This function clears the Bloom filter's bit array, effectively removing all keys from the filter.
 * @param bloom_filter Pointer to the bloom_filter_t structure representing the Bloom filter to be reset.
 * @return Returns 0 on success, or a non-zero error code on failure (e.g., if the bloom_filter pointer is NULL).
 */
int reset_bloom_filter(bloom_filter_t* bloom_filter);

#endif // BLOOM_FILTER_OPERATION_H