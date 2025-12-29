#ifndef HASH_BUCKET_TYPE_DEFINITION_H
#define HASH_BUCKET_TYPE_DEFINITION_H

// Forward declaration for linked_list_node
typedef struct linked_list_node linked_list_node;


#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <pthread.h>


/**
 * @struct key_value_pair
 * @brief Represents a key-value pair used in the keystore.
 *
 * This structure encapsulates a key as a string and its associated value as a byte array,
 * along with the size of the value. It is used for passing key-value data within the keystore system.
 *
 * Fields:
 *   - key: Pointer to the null-terminated string representing the key.
 *   - value: Pointer to the byte array representing the value.
 *   - value_size: Size of the value in bytes.
 */
typedef struct{
    char* key;
    unsigned char* value;
    size_t value_size;
} key_value_pair;

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
 *   - max_linked_list_Chain_length: Maximum allowed length of linked list chains in buckets (sub hash table will be resized if exceeded).
 */
typedef struct
{
    bool is_concurrency_enabled;
    unsigned int bucket_size;
    int max_linked_list_Chain_length;
}sub_hash_table_configuration;


/**
 * @struct hash_table_configuration
 * @brief Configuration parameters for initializing a hash table.
 *
 * This structure contains various configuration settings used during the creation
 * and initialization of a hash table, including bucket size, concurrency settings,
 * sub-hash-table block size, and maximum linked list chain length.
 *
 * Fields:
 *   - bucket_size: Number of buckets in the hash table.
 *   - is_concurrency_enabled: Flag to enable or disable concurrency control.
 *   - sub_hash_table_block_size: Size of each block in the sub-hash-table.
 *   - max_linked_list_Chain_length: Maximum allowed length of linked list chains in buckets (sub hash table will be resized if exceeded).
 */
typedef struct
{
    unsigned int bucket_size;
    bool is_concurrency_enabled;
    unsigned int sub_hash_table_block_size;
    unsigned int max_linked_list_Chain_length;
} hash_table_configuration;




#pragma region Data Structure Definitions

/**
 * @struct data_node
 * @brief Represents a key-value entry in the distributed keystore.
 *
 * The data_node structure stores the key, its hash, the value, and metadata for concurrency control.
 * It is used as the fundamental storage unit within hash buckets and linked lists.
 *
 * Fields: 
 *   - key_hash: Hash of the key (immutable).
 *   - data: Pointer to the value data.
 *   - data_size: Size of the value data in bytes.
 *   - is_concurrency_enabled: Indicates if concurrency protection is enabled for this node.
 *   - is_deleted: Flag indicating if the node has been soft deleted.
 *   - lock: Mutex for concurrency control (if enabled).
 *   - key[]: Flexible array member for storing the key string.
 */
typedef struct data_node
{
    uint32_t key_hash;
    unsigned char *data;
    size_t data_size;
    bool is_concurrency_enabled;
    pthread_mutex_t lock;
    bool is_deleted;
    char key[];
} data_node;


/**
 * @struct linked_list_node
 * @brief Represents a node in a linked list used for collision handling in hash buckets.
 *
 * The linked_list_node structure is used to create chains of data nodes within a hash bucket
 * when collisions occur. Each node contains the key hash, a pointer to the associated data node,
 * and a pointer to the next node in the list.
 *
 * Fields:
 *   - key_hash: Hash of the key (immutable).
 *   - count: Number of data nodes in this linked list chain.
 *   - data_node_ptr: Pointer to the associated data_node.
 *   - next_node: Pointer to the next linked_list_node in the chain.
 */
typedef struct linked_list_node
{
    uint32_t key_hash;
    data_node* data_node_ptr;
    linked_list_node* next_node_ptr;
} linked_list_node;

#pragma endregion

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
 *   - sub_hash_bucket_lock: Read-write lock for synchronizing access to the bucket.
 */
typedef struct 
{
    linked_list_node* linked_list_head;
    unsigned int active_node_count;
    unsigned int total_node_count;
    bool is_initialized;
    bool is_concurrency_enabled;
    unsigned int max_linked_list_Chain_length;
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
    int max_linked_list_Chain_length;
} sub_hash_table_memory_pool;

#pragma endregion

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
    linked_list_node* pending_list_head;
    pthread_mutex_t  resizing_lock;
    unsigned int node_count;
    bool is_resizing;
    bool is_initialized;
    sub_hash_table_configuration config;
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
    sub_hash_table_configuration config;
} hash_table_memory_pool;

#pragma endregion


#endif // HASH_BUCKET_TYPE_DEFINITION_H