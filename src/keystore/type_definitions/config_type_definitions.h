#ifndef CONFIG_TYPE_DEFINITIONS_H
#define CONFIG_TYPE_DEFINITIONS_H

/**
 * @struct sub_hash_table_configuration
 * @brief Configuration parameters for initializing a sub-hash-table.
 *
 * This structure contains various configuration settings used during the creation
 * and initialization of a sub-hash-table, including bucket size, concurrency settings,
 * and maximum linked list chain length.
 *
 * Fields:
 *   - is_concurrency_enabled: Flag to enable or disable concurrency control.
 *   - bucket_size: Number of buckets in the sub-hash-table.
 *   - max_linked_list_chain_length: Maximum allowed length of linked list chains in buckets (sub hash table will be resized if exceeded).
 */
typedef struct
{
    bool is_concurrency_enabled;
    unsigned int bucket_size;
    unsigned int max_linked_list_chain_length;
}sub_hash_table_configuration;


/**
 * @struct hash_table_configuration
 * @brief Configuration parameters for initializing a hash table.
 *
 * This structure contains various configuration settings used during the creation
 * and initialization of a hash table, including bucket size, concurrency settings,
 * sub-hash-table bucket size, and maximum linked list chain length.
 *
 * Fields:
 *   - bucket_size: Number of buckets in the hash table.
 *   - is_concurrency_enabled: Flag to enable or disable concurrency control.
 *   - sub_hash_table_bucket_size: Size of each bucket in the sub-hash-table.
 *   - max_linked_list_chain_length: Maximum allowed length of linked list chains in buckets (sub hash table will be resized if exceeded).
 */
typedef struct
{
    unsigned int bucket_size;
    bool is_concurrency_enabled;
    unsigned int sub_hash_table_bucket_size;
    unsigned int max_linked_list_chain_length;
} hash_table_configuration;


/**
 * @struct memory_manager_config
 * @brief Configuration parameters for initializing the memory manager.
 *
 * This structure contains various configuration settings used during the initialization
 * of the memory manager, including bucket sizes, pre-allocation factors, and concurrency settings.
 *
 * Fields:
 *   - bucket_size: Size of the main memory pool buckets.
 *   - sub_bucket_size: Size of the sub memory pool buckets.
 *   - pre_allocation_factor: Factor determining the amount of memory to pre-allocate.
 *   - allocate_list_pool: Flag to enable or disable allocation from a linked list memory pool.
 *   - is_concurrency_enabled: Flag to enable or disable concurrency control in memory management.
 */
typedef struct memory_manager_config {
    unsigned int bucket_size;
    unsigned int sub_bucket_size;
    double pre_allocation_factor;
    bool allocate_list_pool;
    bool is_concurrency_enabled;
} memory_manager_config;



#endif // CONFIG_TYPE_DEFINITIONS_H