#ifndef CUSTOM_TYPE_DEFINITIONS_H
#define CUSTOM_TYPE_DEFINITIONS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <pthread.h>


// Forward declaration for linked_list_node
typedef struct linked_list_node linked_list_node;


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
 * @struct memory_pool
 * @brief Represents a memory pool for efficient memory management.
 *
 * The memory_pool structure manages a pool of memory blocks to optimize allocation and deallocation.
 * It includes metadata about the pool, such as block size, total blocks, and concurrency control.
 *
 * Fields:
 *   - block_size: Size of each block in bytes.
 *   - pool_start_ptr: Pointer to the start of the pool memory.
 *   - pool_end_ptr: Pointer to the end of the pool memory.
 *   - next_block_ptr: Pointer to the next available block for allocation.
 *   - total_blocks: Total number of blocks in the pool.
 *   - available_blocks: Number of blocks currently available for allocation.
 *   - reusable_blocks: Number of blocks available for reuse.
 *   - free_block_list: Array of pointers to free blocks for quick access.
 *   - is_initialized: Flag indicating if the pool has been initialized.
 *   - pool_lock: Mutex for thread-safe access to the memory pool.
 */
typedef struct memory_pool {
    
    size_t block_size; // Size of each block
    char* pool_start_ptr; // Pointer to the start of the pool memory
    char* pool_end_ptr;   // Pointer to the end of the pool memory
    void *next_block_ptr; // Pointer to the next available block

    unsigned int total_blocks; // Total number of blocks in the pool
    unsigned int available_blocks; // Number of blocks available for allocation
    unsigned int reusable_blocks;  // Number of blocks available for reuse

    void ** free_block_list; // Array of pointers to free blocks
    bool is_initialized; // Flag to indicate if the pool is initialized

    pthread_mutex_t pool_lock; // Mutex for thread-safe access
} memory_pool;


#endif // CUSTOM_TYPE_DEFINITIONS_H