#include "sub_hash_table_operation.h"
#include "utils/memory_manager.h"
#include "utils/helper_functions.h"
#include "sub_hash_bucket_operation.h"


#pragma region Private Function Declarations
static int _get_sub_hash_table_bucket(sub_hash_table_memory_pool* sub_hash_table_ptr, uint32_t key_hash, sub_hash_bucket** sub_hash_bucket_out);
#pragma endregion


#pragma region Public Function Definitions

int create_new_sub_hash_table(sub_hash_table_configuration config, bool earlyInitialize, sub_hash_table_memory_pool** sub_hash_table_out)
{
    if (!is_power_of_two(config.bucket_size))  return ERR_INVALID_CONFIG; // Error handling: bucket_size must be a power of two

    sub_hash_table_memory_pool* new_memory_pool_ptr = allocate_memory(sizeof(sub_hash_table_memory_pool));
    if (new_memory_pool_ptr == NULL) return ERR_MEMORY_ALLOCATION_FAILED; // Error handling: memory allocation failed

    new_memory_pool_ptr->block_size = sizeof(sub_hash_bucket);
    new_memory_pool_ptr->is_initialized = false;
    new_memory_pool_ptr->total_blocks = config.bucket_size;

    new_memory_pool_ptr->sub_hash_buckets_ptr = allocate_memory(config.bucket_size * sizeof(sub_hash_bucket));

    if (new_memory_pool_ptr->sub_hash_buckets_ptr == NULL)  return ERR_MEMORY_ALLOCATION_FAILED; // Error handling: memory allocation failed  
    new_memory_pool_ptr->is_initialized = true;
    new_memory_pool_ptr->is_concurrency_enabled = config.is_concurrency_enabled;
    // Eager initialization of hash buckets if concurrency is enabled or else lazy initialization will be done
    int init_result = 0;
    if (config.is_concurrency_enabled || earlyInitialize) {
        for (unsigned int i = 0; i < config.bucket_size; ++i) {
            init_result = initialise_sub_hash_bucket(&new_memory_pool_ptr->sub_hash_buckets_ptr[i], config.is_concurrency_enabled);
            if (init_result != 0) {
                cleanup_sub_hash_table(new_memory_pool_ptr);
                return init_result; // Error handling: failed to initialize hash bucket
            }
        }
    }

    *sub_hash_table_out = new_memory_pool_ptr;
    return 0;
}

int cleanup_sub_hash_table(sub_hash_table_memory_pool* sub_hash_table_ptr)
{
    if (sub_hash_table_ptr == NULL || !sub_hash_table_ptr->is_initialized) {
        return 0; // Nothing to clean up
    }

    for (unsigned int i = 0; i < sub_hash_table_ptr->total_blocks; ++i) {
        cleanup_sub_hash_bucket(&sub_hash_table_ptr->sub_hash_buckets_ptr[i]);
    }

    free_memory(sub_hash_table_ptr->sub_hash_buckets_ptr, false);
    sub_hash_table_ptr->sub_hash_buckets_ptr = NULL;
    sub_hash_table_ptr->is_initialized = false;
    sub_hash_table_ptr->is_concurrency_enabled = false;
    sub_hash_table_ptr->total_blocks = 0;
    return 0;
}

int upsert_node_to_sub_hash_table(sub_hash_table_memory_pool* sub_hash_table_ptr, uint32_t key_hash, key_value_pair* new_value)
{
    int upsert_result = 0;
    if(new_value == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input
    
    sub_hash_bucket* target_sub_hash_bucket;
    upsert_result = _get_sub_hash_table_bucket(sub_hash_table_ptr, key_hash, &target_sub_hash_bucket);
    if (upsert_result != 0) return upsert_result;

    sub_hash_bucket_operation_args bucket_args = {target_sub_hash_bucket, new_value->key, key_hash};

    int upsert_node_result = update_node_in_sub_hash_bucket(bucket_args, new_value);

    if(upsert_node_result == ERR_DATA_NODE_NOT_FOUND) // Node not found, add new node
    {
        upsert_node_result = add_node_to_sub_hash_bucket(bucket_args, new_value);
    }

    return upsert_node_result;
}

int get_key_store_value_from_sub_hash_table(sub_hash_table_memory_pool* sub_hash_table_ptr, uint32_t key_hash, const char* key, key_value_pair* value_out)
{
    if(value_out == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input

    sub_hash_bucket* target_sub_hash_bucket;
    int get_result = _get_sub_hash_table_bucket(sub_hash_table_ptr, key_hash, &target_sub_hash_bucket);
    if (get_result != 0) return get_result;

    sub_hash_bucket_operation_args bucket_args = {target_sub_hash_bucket, key, key_hash};

    return get_key_store_value_from_sub_hash_bucket(bucket_args, value_out);
}

int delete_key_from_sub_hash_table(sub_hash_table_memory_pool* sub_hash_table_ptr, uint32_t key_hash, const char* key)
{
    sub_hash_bucket* target_sub_hash_bucket;
    int delete_result = _get_sub_hash_table_bucket(sub_hash_table_ptr, key_hash, &target_sub_hash_bucket);
    if (delete_result != 0) return delete_result;

    sub_hash_bucket_operation_args bucket_args = {target_sub_hash_bucket, key, key_hash};

    return delete_key_from_sub_hash_bucket(bucket_args);
}
#pragma endregion


# pragma region Private Function Definitions

/**
 * @fn _get_sub_hash_table_bucket
 * @brief Retrieves the sub-hash-bucket corresponding to the given key hash from the sub-hash-table.
 *
 * This function calculates the index of the sub-hash-bucket using the key hash,
 * checks if it is initialized, and initializes it if necessary.
 *
 * @param args Structure containing sub-hash-table pointer, key, and key hash.
 * @param sub_hash_bucket_out Pointer to receive the retrieved sub-hash-bucket.
 * @return int Returns 0 on success, or a negative error code on failure.
 */
int _get_sub_hash_table_bucket(sub_hash_table_memory_pool* sub_hash_table_ptr, uint32_t key_hash, sub_hash_bucket** sub_hash_bucket_out)
{
    if(sub_hash_table_ptr == NULL || key_hash == 0) return ERR_INVALID_ARGUMENT; // Error handling: invalid input

    if (!sub_hash_table_ptr->is_initialized) return ERR_SUB_HASH_TABLE_NOT_INITIALIZED; // Error handling: sub-hash-table not initialized
    unsigned int index = key_hash & (sub_hash_table_ptr->total_blocks - 1);

    if(index >= sub_hash_table_ptr->total_blocks) return ERR_INVALID_SUB_BUCKET_INDEX; // Error handling: index out of bounds

    sub_hash_bucket* sub_hash_bucket_ptr = &sub_hash_table_ptr->sub_hash_buckets_ptr[index];

    if(!sub_hash_bucket_ptr->is_initialized)
    {
        int init_result = initialise_sub_hash_bucket(sub_hash_bucket_ptr, sub_hash_table_ptr->is_concurrency_enabled);
        if (init_result != 0) {
            cleanup_sub_hash_bucket(sub_hash_bucket_ptr);
            return init_result;
        }
    }

    *sub_hash_bucket_out = sub_hash_bucket_ptr;
    return 0;
}


#pragma endregion