#include <string.h>
#include "hash_functions.h"

// 64-bit constants for MurmurHash3 64-bit variant
static const uint32_t hash_size = 64;
static const uint32_t block_size = 8; // 8 bytes = 64 bits
static const uint64_t block_mix_constant_1 = 0x87c37b91114253d5ULL;
static const uint64_t block_mix_constant_2 = 0x4cf5ad432745937fULL;
static const uint32_t block_rotation_bits = 31;

static const uint32_t hash_rotation_bits = 27;
static const uint64_t hash_multiplier = 5;
static const uint64_t hash_addition_constant = 0x52dce729ULL;

static const uint32_t tailing_bytes_shift_1 = 8;
static const uint32_t tailing_bytes_shift_2 = 16;
static const uint32_t tailing_bytes_shift_3 = 24;
static const uint32_t tailing_bytes_shift_4 = 32;
static const uint32_t tailing_bytes_shift_5 = 40;
static const uint32_t tailing_bytes_shift_6 = 48;

static const uint32_t finalization_shift_1 = 33;
static const uint32_t finalization_shift_2 = 29;
static const uint64_t finalization_multiplier_1 = 0xff51afd7ed558ccdULL;
static const uint64_t finalization_multiplier_2 = 0xc4ceb9fe1a85ec53ULL;

// function declarations
uint64_t left_circular_rotate(uint64_t data, uint32_t rotation_bits, uint32_t hash_size);
uint64_t process_block_data_to_hash(uint64_t block_data, uint64_t hash);
uint64_t process_blocks(const int block_count, uint64_t hash, const uint8_t *data);
uint64_t process_tailing_bytes(int block_count, uint64_t hash, const uint8_t *data, size_t len);


/**
 * Performs a left circular rotation on a 32-bit unsigned integer.
 *
 * @param data The 32-bit unsigned integer to rotate.
 * @param rotation_bits The number of bits to rotate to the left.
 * @param hash_size The bit-width of the hash (typically 32).
 * @return The result of the left circular rotation.
 */
uint64_t left_circular_rotate(uint64_t data, uint32_t rotation_bits, uint32_t hash_size) {
    return (data << rotation_bits) | (data >> (hash_size - rotation_bits));
}


/**
 * @brief Processes a block of data and updates the hash value.
 *
 * This function applies a series of transformations to the input block data,
 * including multiplication by mix constants and a left circular rotation,
 * then combines the result with the current hash value using XOR.
 *
 * @param block_data The input data block to be processed.
 * @param hash The current hash value to be updated.
 * @return The updated hash value after processing the block data.
 */
uint64_t process_block_data_to_hash(uint64_t block_data, uint64_t hash)
{
    block_data *= block_mix_constant_1;
    block_data = left_circular_rotate(block_data, block_rotation_bits, hash_size);
    block_data *= block_mix_constant_2;
    hash ^= block_data;
    return hash;
}



/**
 * @brief Processes a sequence of data blocks to update the hash value.
 *
 * This function iterates over a specified number of blocks (blocks are processed in reverse order for better performance),
 * updating the hash value for each block using a custom block processing function, a left circular rotation,
 * and a final mixing step. The blocks are interpreted as 32-bit unsigned integers.
 *
 * @param block_count The number of blocks to process.
 * @param hash The initial hash value to be updated.
 * @param data Pointer to the input data buffer containing the blocks.
 * @return The updated hash value after processing all blocks.
 */
uint64_t process_blocks(const int block_count, uint64_t hash, const uint8_t *data) {
    const uint64_t *blocks = (const uint64_t*)(data);
    for (int i = 0; i < block_count; i++) {
        uint64_t block_data = blocks[i];
        hash = process_block_data_to_hash(block_data, hash);
        hash = left_circular_rotate(hash, hash_rotation_bits, hash_size);
        hash = hash * hash_multiplier + hash_addition_constant;
    }
    return hash;
}


/**
 * @brief Processes the remaining bytes (tailing bytes) of the input data that do not fit into a full block.
 * 
 * This function handles the bytes left after dividing the input data into blocks of size `block_size`.
 * It combines the remaining bytes into a single 32-bit value and updates the hash using `process_block_data_to_hash`.
 * 
 * @param block_count The number of complete blocks processed so far.
 * @param hash The current hash value to be updated.
 * @param data Pointer to the input data buffer.
 * @param len The total length of the input data.
 * @return The updated hash value after processing the tailing bytes.
 */
uint64_t process_tailing_bytes(const int block_count, uint64_t hash, const uint8_t *data, size_t len) {
    const uint8_t *tail = (const uint8_t *)(data + block_count * block_size);
    int tail_bytes = len & (block_size - 1);
    if (!tail_bytes) {
        return hash;
    }
    uint64_t block_data = 0;
    switch (tail_bytes) {
        case 7: block_data ^= ((uint64_t)tail[6]) << tailing_bytes_shift_6; // fall through
        case 6: block_data ^= ((uint64_t)tail[5]) << tailing_bytes_shift_5; // fall through
        case 5: block_data ^= ((uint64_t)tail[4]) << tailing_bytes_shift_4; // fall through
        case 4: block_data ^= ((uint64_t)tail[3]) << tailing_bytes_shift_3; // fall through
        case 3: block_data ^= ((uint64_t)tail[2]) << tailing_bytes_shift_2; // fall through
        case 2: block_data ^= ((uint64_t)tail[1]) << tailing_bytes_shift_1; // fall through
        case 1: block_data ^= ((uint64_t)tail[0]);
    }
    hash = process_block_data_to_hash(block_data, hash);
    return hash;
}



/**
 * @brief Performs finalization steps on a hash value to improve its distribution.
 *
 * This function applies a series of bitwise operations and multiplications to the input hash,
 * which helps to further mix the bits and reduce hash collisions. It is typically used as the
 * last step in a hash function implementation.
 *
 * @param hash The initial hash value to be finalized.
 * @return The finalized hash value.
 */
uint64_t finalization(uint64_t hash) {
    hash ^= hash >> finalization_shift_1;
    hash *= finalization_multiplier_1;
    hash ^= hash >> finalization_shift_2;
    hash *= finalization_multiplier_2;
    hash ^= hash >> finalization_shift_1;
    return hash;
}



/**
 * @brief Computes a 64-bit MurmurHash for the given key.
 *
 * This function applies the MurmurHash algorithm to the input string `key`
 * using the specified `seed`. It processes the input in 4-byte blocks,
 * handles any remaining tailing bytes, and performs finalization steps
 * to produce the hash value.
 *
 * @param key   The input string to hash.
 * @param seed  The seed value for the hash function.
 * @return      The resulting 64-bit hash value or UINT64_MAX on error.
 */
uint64_t hash_function_murmur_64(const char *key,  uint64_t seed) {
    //Validation
    if (key == NULL) {
        return UINT64_MAX;  // Return error code for NULL keys
    }
    // Initialize
    const uint8_t *data = (const uint8_t *)key;
    size_t len = strlen(key);
    const int block_count = len / block_size; // Number of 8-byte blocks
    uint64_t hash = seed;
    hash = process_blocks(block_count, hash, data);
    hash = process_tailing_bytes(block_count, hash, data, len);
    hash = finalization(hash);
    return hash;
}

