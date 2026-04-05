#include "bloom_filter_operation.h"
#include "type_definitions/sucess_code_definitions.h"
#include "type_definitions/error_code_definitions.h"
#include "utils/memory_manager.h"
#include "utils/helper_functions.h"

#pragma region  private data structure declarations
static int _seed_hash_functions(bloom_filter_t* bloom_filter);
static int _create_bit_array(bloom_filter_t* bloom_filter, uint32_t max_chain_length);
static int _probabilistic_check_function(composite_key_hash key_hash, bloom_filter_t* bloom_filter, bool* result_out);
static int _probabilistic_add_function(composite_key_hash key_hash, bloom_filter_t* bloom_filter);
static int _get_bit_mask(uint64_t hash1, uint64_t hash2, uint32_t bit_array_size, int index, uint32_t* byte_index_out, uint8_t* bit_mask_out);
#pragma endregion


#pragma region public function definitions


// --- Constants for clarity ---
#define BLOOM_FILTER_MIN_CHAIN_LENGTH 7
#define BLOOM_FILTER_MAX_CHAIN_LENGTH 30
#define BLOOM_FILTER_ERR_OUT_OF_BOUNDS ERR_INVALID_ARGUMENT

int initialize_bloom_filter(uint32_t max_chain_length, bloom_filter_t** bloom_filter_out)
{
    if (bloom_filter_out == NULL) return ERR_INVALID_ARGUMENT;
    *bloom_filter_out = NULL;

    // The minimum chain length for a Bloom filter to be effective is typically around 7.
    if (max_chain_length < BLOOM_FILTER_MIN_CHAIN_LENGTH || max_chain_length > BLOOM_FILTER_MAX_CHAIN_LENGTH) {
        return BLOOM_FILTER_ERR_OUT_OF_BOUNDS;
    }

    bloom_filter_t* bloom_filter = callocate_memory(1, sizeof(bloom_filter_t));
    if (bloom_filter == NULL) return ERR_MEMORY_ALLOCATION_FAILED;

    int result = 0;

    result = _seed_hash_functions(bloom_filter);
    if (result != 0) {
        free(bloom_filter);
        return result;
    }

    result = _create_bit_array(bloom_filter, max_chain_length);
    if (result != 0) {
        free(bloom_filter);
        return result;
    }

    bloom_filter->bit_array = callocate_memory((bloom_filter->bit_array_size + 7) / 8, sizeof(uint8_t));
    if (bloom_filter->bit_array == NULL) {
        free(bloom_filter);
        return ERR_MEMORY_ALLOCATION_FAILED;
    }

    *bloom_filter_out = bloom_filter;
    return SUCCESS;
}

int cleanup_bloom_filter(bloom_filter_t* bloom_filter)
{
    if (bloom_filter == NULL) return SUCCESS;
    if (bloom_filter->bit_array != NULL) {
        free(bloom_filter->bit_array);
        bloom_filter->bit_array = NULL;
    }
    free(bloom_filter);
    return SUCCESS;
}

int add_key_to_bloom_filter(composite_key_hash key_hash, bloom_filter_t* bloom_filter)
{
    if (bloom_filter == NULL) return ERR_INVALID_ARGUMENT;
    if (bloom_filter->bit_array == NULL) return ERR_INVALID_ARGUMENT;
    return _probabilistic_add_function(key_hash, bloom_filter);
}

int check_key_in_bloom_filter(composite_key_hash key_hash, bloom_filter_t* bloom_filter, bool* result_out)
{
    if (bloom_filter == NULL || result_out == NULL) return ERR_INVALID_ARGUMENT;
    if (bloom_filter->bit_array == NULL) return ERR_INVALID_ARGUMENT;
    return _probabilistic_check_function(key_hash, bloom_filter, result_out);
}

#pragma endregion

#pragma region private function definitions

int _seed_hash_functions(bloom_filter_t* bloom_filter)
{
    // This function will generate hash seeds for the Bloom filter, which will be used to create multiple hash functions.
    bloom_filter->hash_seed1 = generate_hash_seed();
    bloom_filter->hash_seed2 = derive_distinct_seed(generate_hash_seed());
    return SUCCESS;
}

/**
 * @fn _create_bit_array
 * @brief Creates the bit array for the Bloom filter based on the maximum chain length of a sub-bucket.
 * This function determines the optimal number of hash functions and the size of the bit array based on
 * the provided maximum chain length. It sets the parameters in the bloom_filter structure accordingly.
 * @param bloom_filter Pointer to the bloom_filter_t structure where the parameters will be set.
 * @param max_chain_length The maximum number of entries expected in a sub-bucket, used to determine the size of the Bloom filter.
 * @return Returns 0 on success, or a non-zero error code on failure (e.g., if the bloom_filter pointer is NULL or if the max_chain_length is out of expected bounds).
 * Note: The function should handle cases where the max_chain_length is less than or equal to 6 (where a Bloom filter may not be effective) and greater than 50 (where the Bloom filter may become too large), returning appropriate error codes in those cases.
 */
int _create_bit_array(bloom_filter_t* bloom_filter, uint32_t max_chain_length)
{
    // Use named constants for clarity
    if (max_chain_length <= 10) {
        bloom_filter->num_hashes = 2;
        bloom_filter->bit_array_size = 32;    // Minimal for very short chains
    } else if (max_chain_length <= 20) {
        bloom_filter->num_hashes = 3;
        bloom_filter->bit_array_size = 64;    // Sweet spot: best tradeoff
    } else if (max_chain_length <= BLOOM_FILTER_MAX_CHAIN_LENGTH) {
        bloom_filter->num_hashes = 4;
        bloom_filter->bit_array_size = 128;   // Large chains
    } else {
        // Out of bounds
        return BLOOM_FILTER_ERR_OUT_OF_BOUNDS;
    }
    return SUCCESS;
}

/**
 * @fn _probabilistic_check_function
 * @brief Checks if a key is likely present in the Bloom filter using a probabilistic approach.
 * This function uses the Bloom filter's hash functions to check if the specified key is likely present.
 * It provides a probabilistic result, meaning that it may indicate that a key is present when it is not (false positive), 
 * but it will never indicate that a key is absent if it is indeed present (no false negatives).
 * @param key_hash The hash of the key to be checked in the Bloom filter.
 * @param bloom_filter Pointer to the bloom_filter_t structure representing the Bloom filter.
 * @param result_out Pointer to a boolean variable where the result will be stored. 
 * True indicates the key is likely present, false indicates it is definitely not present.
 * @return Returns 0 on success, or a non-zero error code on failure (e.g., if the bloom_filter pointer is NULL).
 */
int _probabilistic_check_function(composite_key_hash key_hash, bloom_filter_t* bloom_filter, bool* result_out)
{
    // Double hashing: h_i = (hash1 + i * hash2) % m
    uint64_t hash1 = key_hash.sub_bucket_hash ^ bloom_filter->hash_seed1;
    uint64_t hash2 = key_hash.sub_bucket_hash ^ bloom_filter->hash_seed2;
    uint32_t byte_index;
    uint8_t bit_mask;
    for (int i = 0; i < bloom_filter->num_hashes; i++) {
        _get_bit_mask(hash1, hash2, bloom_filter->bit_array_size, i, &byte_index, &bit_mask);
        if ((bloom_filter->bit_array[byte_index] & bit_mask) == 0) {
            *result_out = false;
            return SUCCESS;
        }
    }
    *result_out = true;
    return SUCCESS;
}

/**
 * @fn _probabilistic_add_function
 * @brief Adds a key to the Bloom filter using a probabilistic approach.
 * This function updates the Bloom filter's bit array based on the provided key, using the filter's hash functions.
 * It calculates the appropriate bit positions in the Bloom filter's bit array for the given key hash and sets those bits to indicate the presence of the key.
 * @param key_hash The hash of the key to be added to the Bloom filter.
 * @param bloom_filter Pointer to the bloom_filter_t structure representing the Bloom filter.
 * @return Returns 0 on success, or a non-zero error code on failure (e.g., if the bloom_filter pointer is NULL).
 */
int _probabilistic_add_function(composite_key_hash key_hash, bloom_filter_t* bloom_filter)
{
    uint64_t hash1 = key_hash.sub_bucket_hash ^ bloom_filter->hash_seed1;
    uint64_t hash2 = key_hash.sub_bucket_hash ^ bloom_filter->hash_seed2;
    uint32_t byte_index;
    uint8_t bit_mask;
    for (int i = 0; i < bloom_filter->num_hashes; i++) {
        _get_bit_mask(hash1, hash2, bloom_filter->bit_array_size, i, &byte_index, &bit_mask);
        bloom_filter->bit_array[byte_index] |= bit_mask;
    }
    return SUCCESS;
}

/**
 * @fn _get_bit_mask
 * @brief Calculates the byte index and bit mask for a given key hash and Bloom filter parameters.
 * This function computes the appropriate byte index and bit mask for a given key hash based on the Bloom filter's hash functions and bit array size. 
 * It uses double hashing to determine the bit positions in the Bloom filter's bit array that correspond to the key hash.
 * @param hash1 The first hash value derived from the key hash and Bloom filter's first hash seed.
 * @param hash2 The second hash value derived from the key hash and Bloom filter's second hash seed.
 * @param bit_array_size The size of the Bloom filter's bit array in bits.
 * @param index The index of the hash function being applied (0 to num_hashes-1).
 * @param byte_index_out Pointer to a uint32_t variable where the calculated byte index will be stored.
 * @param bit_mask_out Pointer to a uint8_t variable where the calculated bit mask will be stored.
 * @return Returns 0 on success, or a non-zero error code on failure (e.g., if the bloom_filter pointer is NULL).
 */
int _get_bit_mask(uint64_t hash1, uint64_t hash2, uint32_t bit_array_size, int index, uint32_t* byte_index_out, uint8_t* bit_mask_out) {
    uint32_t bit_index = (uint32_t)((hash1 + index * hash2) % bit_array_size);
    *byte_index_out = bit_index / 8;
    *bit_mask_out = 1 << (bit_index % 8);
    return SUCCESS;
}

#pragma endregion