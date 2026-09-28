#ifndef HELPER_FUNCTIONS_H
#define HELPER_FUNCTIONS_H

#include <stdlib.h>
#include <stdint.h>

/**
 * @fn is_power_of_two
 * @brief Checks if a given unsigned integer is a power of two.
 * @param bucket_size The unsigned integer to check.
 * @return int Returns 1 (true) if the number is a power of two, otherwise returns 0 (false).
 */
int is_power_of_two(unsigned int bucket_size);

/**
 * @fn portable_sleep_ms
 * @brief Cross-platform function to sleep for a specified number of milliseconds.
 * @param ms The number of milliseconds to sleep.
 */
void portable_sleep_ms(unsigned long ms);

/**
 * @fn portable_sleep_us
 * @brief Cross-platform function to sleep for a specified number of microseconds.
 * @param microseconds The number of microseconds to sleep.
 */
void portable_sleep_us(unsigned long microseconds);

/**
 * @fn generate_hash_seed
 * @brief Generates a hash seed using system time.
 * @return uint64_t Returns a 64-bit hash seed.
 * @note This function generates a seed for hash functions using system time, not suitable for cryptographic purposes; entropy is limited and may be predictable.
 */
uint64_t generate_hash_seed();

/**
 * @fn derive_distinct_seed
 * @brief Derives a distinct hash seed from a given base seed.
 * @param base_seed The original hash seed from which to derive a new seed.
 * @return uint64_t Returns a new 64-bit hash seed derived from the base seed.
 * @note This function uses a simple XOR operation with a constant to produce a distinct seed, 
 * which can be useful for generating multiple seeds for different hash functions while maintaining some relationship to the original seed.
 */
uint64_t derive_distinct_seed(uint64_t base_seed);

#endif // HELPER_FUNCTIONS_H