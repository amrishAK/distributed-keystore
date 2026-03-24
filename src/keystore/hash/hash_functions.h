#ifndef HASH_FUNCTIONS_H
#define HASH_FUNCTIONS_H

#include <stdint.h>
#include <stddef.h>

/**
 * @fn hash_function_murmur_64
 * @brief Computes the MurmurHash3 (64-bit) of the given key with the specified seed.
 *
 * This function implements the MurmurHash3 algorithm to generate a 64-bit hash value
 * from the input key string and seed. It is designed for non-cryptographic hashing,
 * providing good distribution and performance for hash table lookups.
 *
 * @param key  The input string to be hashed. If NULL, the function returns 0.
 * @param seed A 64-bit seed value to initialize the hash. Different seeds produce different hashes.
 * @return A 64-bit hash value computed from the input key and seed or UINT64_MAX on error.
 */
uint64_t hash_function_murmur_64(const char *key,  uint64_t seed);

#endif // HASH_FUNCTIONS_H