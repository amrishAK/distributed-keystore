#ifndef HASH_FUNCTIONS_H
#define HASH_FUNCTIONS_H

#include <stdint.h>
#include <stddef.h>

/**
 * @fn hash_function_murmur_32
 * @brief Computes the MurmurHash3 (32-bit) of the given key with the specified seed.
 *
 * This function implements the MurmurHash3 algorithm to generate a 32-bit hash value
 * from the input key string and seed. It is designed for non-cryptographic hashing,
 * providing good distribution and performance for hash table lookups.
 *
 * @param key  The input string to be hashed. If NULL, the function returns 0.
 * @param seed A 32-bit seed value to initialize the hash. Different seeds produce different hashes.
 * @return A 32-bit hash value computed from the input key and seed or UINT32_MAX on error.
 */
uint32_t hash_function_murmur_32(const char *key,  uint32_t seed);

#endif // HASH_FUNCTIONS_H