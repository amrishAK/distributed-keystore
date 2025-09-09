#include <stdlib.h>
#include "hash_buckets.h"
#include "hash_bucket_list.h"
#include "core/type_definition.h"
#include "utils/memory_manager.h"

//global variables
static hash_bucket_memory_pool g_hash_bucket_pool = {0};

//function decleration
static bool _is_power_of_two(unsigned int n);
static void _initialise_hash_bucket(hash_bucket *hash_bucket_ptr);
static void _delete_hash_bucket(unsigned int index);

int initialise_hash_buckets(unsigned int bucket_size) 
{    
    if (!_is_power_of_two(bucket_size)) {
        // Handle error: bucket_size must be a power of two
        return -1;
    }

    g_hash_bucket_pool.block_size = sizeof(hash_bucket);
    g_hash_bucket_pool.is_initialized = false;
    g_hash_bucket_pool.total_blocks = bucket_size;

    g_hash_bucket_pool.hash_buckets_ptr = calloc(bucket_size, sizeof(hash_bucket));

    if (g_hash_bucket_pool.hash_buckets_ptr == NULL) {
        return -1; // Handle error: memory allocation failed
    }

    g_hash_bucket_pool.is_initialized = true;

    return 0;
}


int cleanup_hash_buckets(void) 
{
    if(g_hash_bucket_pool.is_initialized) {
        for(unsigned int i = 0; i < g_hash_bucket_pool.total_blocks; i++) {
            _delete_hash_bucket(i);
        }
        free(g_hash_bucket_pool.hash_buckets_ptr);
        g_hash_bucket_pool.hash_buckets_ptr = NULL;
        g_hash_bucket_pool.block_size = 0;
        g_hash_bucket_pool.total_blocks = 0;
        g_hash_bucket_pool.is_initialized = false;
        g_hash_bucket_pool = (hash_bucket_memory_pool){0};
    }
    return 0;
}


hash_bucket*  get_hash_bucket(unsigned int index, bool create_if_missing) {

    if(!g_hash_bucket_pool.is_initialized || index >= g_hash_bucket_pool.total_blocks) {
        return NULL; // Memory pool not initialized or index out of bounds
    }

    hash_bucket* target_bucket_ptr =  &g_hash_bucket_pool.hash_buckets_ptr[index];

    if(target_bucket_ptr->is_initialized) {
        return target_bucket_ptr; // Bucket already exists and is initialized
    }
    else
    {
        _initialise_hash_bucket(target_bucket_ptr);
    }

    return target_bucket_ptr;
}


int add_node_to_bucket(unsigned int index, uint32_t key_hash, data_node *data_node_ptr)
{
    hash_bucket *hash_bucket_ptr = get_hash_bucket(index, true);


    if (hash_bucket_ptr == NULL) {
        
        return -1; // Bucket does not exist
    }

    switch (hash_bucket_ptr->type)
    {
        case BUCKET_LIST:
            return insert_list_node(&hash_bucket_ptr->container.list, key_hash, data_node_ptr);
        default:
            return -1; // Unsupported bucket type
    }

    return 0;
}

data_node *find_node_in_bucket(unsigned int index, const char *key, uint32_t key_hash)
{
    hash_bucket *hash_bucket_ptr = get_hash_bucket(index, false);

    if (hash_bucket_ptr == NULL) {
        return NULL; // Handle error: invalid bucket
    }

    switch (hash_bucket_ptr->type)
    {
        case BUCKET_LIST: 
            list_node *found_node = find_list_node(hash_bucket_ptr->container.list, key, key_hash);
            return found_node != NULL ? found_node->data : NULL;
        default: return NULL; // Unsupported bucket type
    }
}

int delete_node_from_bucket(unsigned int index, const char *key, uint32_t key_hash)
{
    hash_bucket *hash_bucket_ptr = get_hash_bucket(index, false);

    if (hash_bucket_ptr == NULL) {
        return -1; // Handle error: invalid bucket
    }

    switch (hash_bucket_ptr->type)
    {
        case BUCKET_LIST: 
            return delete_list_node(&hash_bucket_ptr->container.list, key, key_hash);
        default: return -1; // Unsupported bucket type
    }
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
bool _is_power_of_two(unsigned int n) {
    return n > 0 && (n & (n - 1)) == 0;
}


void _initialise_hash_bucket(hash_bucket *hash_bucket_ptr) {
    hash_bucket_ptr->type = BUCKET_LIST;
    hash_bucket_ptr->container.list = NULL;
    hash_bucket_ptr->count = 0;
    hash_bucket_ptr->is_initialized = true;
}

void _delete_hash_bucket(unsigned int index) {

    hash_bucket *hash_bucket_ptr = get_hash_bucket(index, false);

    if (hash_bucket_ptr == NULL) {
        return; // Bucket does not exist
    }

    switch (hash_bucket_ptr->type)
    {
        case BUCKET_LIST: 
            delete_all_list_nodes(hash_bucket_ptr->container.list);
            hash_bucket_ptr->container.list = NULL;
            break;
        default:
            break;
    }

    hash_bucket_ptr->type = NONE;
    hash_bucket_ptr->count = 0;
    hash_bucket_ptr->is_initialized = false;
}