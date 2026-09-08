#ifndef HASH_BUCKET_TYPE_DEFINITION_H
#define HASH_BUCKET_TYPE_DEFINITION_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <pthread.h>

#include "custom_type_definitions.h"
#include "config_type_definitions.h"


#pragma region Sub Hash Table Definitions

/**
 * @struct sub_hash_bucket
 * @brief Represents a sub-hash-bucket in the sub-hash-table.
 *
 * The sub_hash_bucket structure manages a linked list of data nodes and handles concurrency control.
 * It includes metadata about the bucket, such as node counts and initialization status.
 *
 * Fields:
 *   - linked_list_head: Pointer to the head of the linked list of data nodes.
 *   - active_node_count: Number of active (non-deleted) nodes in the bucket.
 *   - total_node_count: Total number of nodes in the bucket (including deleted).
 *   - is_initialized: Flag to indicate if the bucket has been initialized.
 *   - is_concurrency_enabled: Flag to indicate if concurrency control is enabled for the bucket.
 *   - is_bloom_filter_enabled: Flag to indicate if bloom filter is enabled for the bucket.
 *   - max_linked_list_chain_length: Maximum allowed length of the linked list chain before resizing is triggered.
 *   - bloom_filter_ptr: Pointer to the bloom filter associated with this sub-hash-bucket for quick membership checks.
 *   - sub_hash_bucket_lock: Read-write lock for synchronizing access to the bucket.
 */
typedef struct sub_hash_bucket
{
    linked_list_node* linked_list_head;
    atomic_uint active_node_count; // updated concurrently across threads without holding sub_hash_bucket_lock
    atomic_uint total_node_count;  // updated concurrently across threads without holding sub_hash_bucket_lock
    bool is_initialized;
    bool is_concurrency_enabled;
    bool is_bloom_filter_enabled;
    unsigned int max_linked_list_chain_length;
    bloom_filter_t* bloom_filter_ptr;
    pthread_rwlock_t sub_hash_bucket_lock;
} sub_hash_bucket;

/**
 * @struct sub_hash_table_memory_pool
 * @brief Represents the memory pool for the sub-hash-table.
 *
 * The sub_hash_table_memory_pool structure manages the collection of sub-hash-buckets used in the sub-hash-table.
 * It includes metadata about the pool, such as block size, total blocks, and concurrency settings.
 *
 * Fields:
 *   - sub_hash_buckets_ptr: Pointer to the array of sub-hash-buckets.
 *   - block_size: Size of each block in bytes.
 *   - total_blocks: Total number of blocks in the pool.
 *   - is_initialized: Flag indicating if the pool has been initialized.
 *   - is_concurrency_enabled: Flag indicating if concurrency control is enabled for the pool.
 */
typedef struct
{
    sub_hash_bucket* sub_hash_buckets_ptr;
    unsigned int block_size;
    unsigned int total_blocks;
    bool is_initialized;
    bool is_concurrency_enabled;
    unsigned int max_linked_list_chain_length;
} sub_hash_table_memory_pool;

#pragma endregion


/**
 * @struct resizing_buffer
 * @brief Represents the buffer used during hash table resizing operations.
 *
 * The resizing_buffer structure manages temporary data structures used during the resizing of the hash table.
 * It includes pointers to the new sub-hash-table, buffers for deleted and updated operations, and a buffer for new operations.
 *
 * Fields:
 *   - new_sub_hash_table_ptr: Pointer to the new sub-hash-table memory pool being created during resizing.
 *   - deleted_operation_buffer_head: Head of the linked list buffer for delete operations during resizing.
 *   - updated_operation_buffer_head: Head of the linked list buffer for update operations during resizing.
 *   - new_operation_buffer_ptr: Pointer to the buffer for new operations during resizing.
 */
typedef struct {
    sub_hash_table_memory_pool* new_sub_hash_table_ptr;
    delete_operation_buffer* delete_operation_buffer_ptr;
    linked_list_node* updated_operation_buffer_head;
    new_operation_buffer* new_operation_buffer_ptr;
} resizing_buffer;

#pragma region Hash Table Definitions

/**
 * @struct hash_bucket
 * @brief Represents a hash bucket in the hash table.
 *
 * The hash_bucket structure manages a collection of sub-hash-tables and handles resizing operations.
 * It includes metadata about the bucket, such as node count, resizing status, and concurrency control.
 *
 * Fields:
 *   - sub_hash_table_ptr: Pointer to the sub-hash-table bucket memory pool.
 *   - snapshot_sub_hash_table_ptr: Pointer to the snapshot sub-hash-table bucket memory pool (used during resizing).
 *   - node_count: Number of nodes in the sub-hash-table.
 *   - pending_list_head: Head of the pending list for nodes added during resizing.
 *   - is_resizing: Flag to indicate if resizing is in progress.
 *   - resizing_lock: Spin lock for synchronizing resizing operations.
 *   - is_initialized: Flag to indicate if the pool is initialized.
 */
typedef struct 
{
    sub_hash_table_memory_pool* sub_hash_table_ptr;
    sub_hash_table_memory_pool* snapshot_sub_hash_table_ptr;
    resizing_buffer* resizing_buffer_ptr;
    pthread_mutex_t  resizing_lock;
    unsigned int node_count;
    bool is_resizing;
    bool is_initialized;
    sub_hash_table_configuration sub_hash_table_config;
} hash_bucket;


/**
 * @struct hash_table_memory_pool
 * @brief Represents the memory pool for the hash table.
 *
 * The hash_table_memory_pool structure manages the collection of hash buckets used in the hash table.
 * It includes metadata about the pool, such as block size, total blocks, and concurrency settings.
 *
 * Fields:
 *   - hash_buckets_ptr: Pointer to the array of hash buckets.
 *   - block_size: Size of each block in bytes.
 *   - total_blocks: Total number of blocks in the pool.
 *   - is_initialized: Flag indicating if the pool has been initialized.
 *   - is_concurrency_enabled: Flag indicating if concurrency control is enabled for the pool.
 */
typedef struct 
{
    hash_bucket* hash_buckets_ptr;
    unsigned int block_size;
    unsigned int total_blocks;
    bool is_initialized;
    sub_hash_table_configuration sub_hash_table_config;
} hash_table_memory_pool;

#pragma endregion




#endif // HASH_BUCKET_TYPE_DEFINITION_H