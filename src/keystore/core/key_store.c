#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "key_store.h"
#include "data_node.h"
#include "bucket/hash_buckets.h"
#include "bucket/hash_bucket_list.h"
#include "hash/hash_functions.h"

// global variables
static hash_bucket *g_hash_buckets_ptr = NULL;
static int g_bucket_size = 0;
static int g_hash_seed = 0;

// function declarations
bool is_power_of_two(int n);
int get_bucket_index(uint32_t key_hash);
int get_bucket_and_key_hash(const char *key, hash_bucket **bucket_out, uint32_t *key_hash_out);


/**
 * @fn initialise_key_store
 * @brief Initializes the key store with the specified bucket size.
 *
 * This function sets up the global hash bucket array for the key store.
 * The bucket size must be a power of two. Memory is allocated for the
 * hash buckets, and the global bucket size is set.
 *
 * @param bucket_size The number of buckets to allocate (must be a power of two).
 * @return 0 on success, -1 on failure (invalid bucket size or memory allocation failure).
 */
int initialise_key_store(int bucket_size) 
{    
    if (!is_power_of_two(bucket_size)) {
        // Handle error: bucket_size must be a power of two
        return -1;
    }

    g_bucket_size = bucket_size;
    g_hash_buckets_ptr = malloc(sizeof(hash_bucket) * bucket_size);

    if (g_hash_buckets_ptr == NULL) {
        return -1; // Handle error: memory allocation failed
    }

    return 0;
}


/**
 * @fn cleanup_key_store
 * @brief Cleans up and releases all resources used by the key store.
 *
 * This function frees the memory allocated for the hash buckets and resets
 * global variables associated with the key store.
 *
 * @return 0 on success.
 */
int cleanup_key_store(void) 
{
    if (g_hash_buckets_ptr != NULL) {
        for (int i = 0; i < g_bucket_size; i++) {
            delete_hash_bucket(&g_hash_buckets_ptr[i]);
        }
        free(g_hash_buckets_ptr);
        g_hash_buckets_ptr = NULL;
    }
    g_bucket_size = 0;
    g_hash_seed = 0;
    return 0;
}

/**
 * @fn set_key
 * @brief Sets a key-value pair in the key store.
 *
 * This function inserts or updates the value associated with the specified key.
 * If the key already exists, its data is updated. If the key does not exist,
 * a new entry is created. The function handles error cases such as invalid
 * input, memory allocation failure, or bucket lookup failure.
 *
 * @param key        The key to set (null-terminated string).
 * @param data       Pointer to the data to associate with the key.
 * @param data_size  Size of the data in bytes.
 *
 * @return 0 on success, -1 on error (invalid input, memory allocation failure, or bucket lookup failure).
 */
int set_key(const char *key, const unsigned char *data, size_t data_size) 
{
    if ( data == NULL || data_size == 0) {
        return -1; // Handle error: invalid key or data
    } 

    hash_bucket *hash_bucket_ptr;
    uint32_t key_hash;
    if (get_bucket_and_key_hash(key, &hash_bucket_ptr, &key_hash) != 0) {
        return -1; // Handle error: failed to get bucket and key hash
    }

    data_node *data_node_ptr = find_node_in_bucket(hash_bucket_ptr, key, key_hash);

    if(data_node_ptr != NULL) {
        return update_data_node(data_node_ptr, data, data_size);
    }
    

    data_node *new_data_node_ptr = create_data_node(key, key_hash, data, data_size);
    if (new_data_node_ptr == NULL) {
        return -1; // Handle error: memory allocation failed
    }

    return add_node_to_bucket(hash_bucket_ptr, key_hash, new_data_node_ptr);
}

/**
 * @fn get_key
 * @brief Retrieves the data node associated with the specified key.
 *
 * This function computes the hash of the given key and locates the corresponding
 * hash bucket. It then searches for the node within the bucket that matches the key.
 *
 * @param key The key to search for in the key store.
 * @return Pointer to the data_node if found; otherwise, returns NULL or an error indicator.
 */
data_node* get_key(const char *key) {
    
    hash_bucket *hash_bucket_ptr;
    uint32_t key_hash;
    if (get_bucket_and_key_hash(key, &hash_bucket_ptr, &key_hash) != 0) {
        return NULL; // Handle error: failed to get bucket and key hash
    }

    return find_node_in_bucket(hash_bucket_ptr, key, key_hash);
}

/**
 * @fn delete_key
 * @brief Deletes a key from the key store.
 *
 * This function locates the hash bucket corresponding to the given key,
 * computes the key's hash, and removes the key from the bucket.
 *
 * @param key The key to be deleted from the key store.
 * @return 0 on success, -1 if the key or bucket could not be found or an error occurred.
 */
int delete_key(const char *key) 
{    
    hash_bucket *hash_bucket_ptr;
    uint32_t key_hash;
    if (get_bucket_and_key_hash(key, &hash_bucket_ptr, &key_hash) != 0) {
        return -1; // Handle error: failed to get bucket and key hash
    }

    return delete_node_from_bucket(hash_bucket_ptr, key, key_hash);
}


/**
 * @fn get_bucket_index
 * @brief Computes the bucket index for a given key hash.
 *
 * This function uses bitwise AND to map the key_hash to a valid bucket index
 * within the range [0, g_bucket_size - 1]. If the computed index is out of bounds,
 * it returns -1 to indicate an invalid index.
 *
 * @param key_hash The hash value of the key.
 * @return The bucket index corresponding to the key_hash, or -1 if invalid.
 */
int get_bucket_index(uint32_t key_hash) 
{
    int index = key_hash & (g_bucket_size - 1);

    if(index < 0 || index >= g_bucket_size) {
        index = -1;
    }
    
    return index;
}



/**
 * @fn is_power_of_two
 * @brief Checks if a given integer is a power of two.
 *
 * This function returns true if the input integer `n` is a power of two,
 * and false otherwise. It works for positive integers only.
 *
 * @param n The integer to check.
 * @return true if `n` is a power of two, false otherwise.
 */
bool is_power_of_two(int n) {
    return n > 0 && (n & (n - 1)) == 0;
}


/**
 * @fn get_bucket_and_key_hash
 * @brief Computes the hash of a given key, determines the corresponding bucket, and outputs both.
 *
 * This function takes a key string, computes its hash using MurmurHash (with a global seed),
 * determines the appropriate hash bucket index, retrieves the bucket pointer, and outputs both
 * the bucket pointer and the key hash value.
 *
 * @param key           The key string to be hashed and located.
 * @param bucket_out    Output pointer to receive the address of the corresponding hash bucket.
 * @param key_hash_out  Output pointer to receive the computed hash value of the key.
 *
 * @return 0 on success, -1 on error (invalid arguments, hash index failure, or bucket not found).
 */
int get_bucket_and_key_hash(const char *key, hash_bucket **bucket_out, uint32_t *key_hash_out) 
{
    if (key == NULL || key[0] == '\0' || bucket_out == NULL || key_hash_out == NULL) {
        return -1; // Handle error: invalid arguments
    }

    uint32_t key_hash = hash_function_murmur_32(key, g_hash_seed);
    int index = get_bucket_index(key_hash);

    if (index < 0) {
        return -1; // Handle error: invalid index
    }

    hash_bucket *hash_bucket_ptr = get_hash_bucket(g_hash_buckets_ptr, index, true);
    if (hash_bucket_ptr == NULL) {
        return -1; // Handle error: bucket does not exist
    }

    *bucket_out = hash_bucket_ptr;
    *key_hash_out = key_hash;
    return 0;
}
