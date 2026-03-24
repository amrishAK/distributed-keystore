#ifndef CUSTOM_TYPE_DEFINITIONS_H
#define CUSTOM_TYPE_DEFINITIONS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <pthread.h>


// Forward declaration for linked_list_node and double_linked_list_node
typedef struct linked_list_node linked_list_node;
typedef struct double_linked_list_node double_linked_list_node;

#pragma region Data Structure Definitions

/**
 * @struct composite_key_hash
 * @brief Represents a composite hash for a key, combining both bucket and sub-bucket hashes.
 *
 * This structure is used to efficiently identify the location of a key within the hash table,
 * especially during resizing operations. It contains the hash of the key for the main bucket
 * and the hash for the sub-bucket, allowing for quick comparisons and lookups.
 *
 * Fields:
 *   - bucket_hash: Hash of the key used to determine the main bucket index.
 *   - sub_bucket_hash: Hash of the key used to determine the sub-bucket index within the main bucket.
*/
typedef struct
{
    uint64_t bucket_hash;
    uint64_t sub_bucket_hash;
} composite_key_hash;

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
    composite_key_hash key_hash;
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
 *   - key_hash: Hash of the key (immutable) - This is the sub-bucket hash of the key, taken from the composite_key_hash structure.
 *   - count: Number of data nodes in this linked list chain.
 *   - data_node_ptr: Pointer to the associated data_node.
 *   - next_node: Pointer to the next linked_list_node in the chain.
 */
typedef struct linked_list_node
{
    uint64_t key_hash;
    data_node* data_node_ptr;
    linked_list_node* next_node_ptr;
} linked_list_node;



/**
 * @struct double_linked_list_node
 * @brief Represents a node in a doubly linked list.
 *
 * The double_linked_list_node structure is used for creating doubly linked lists,
 * allowing traversal in both directions. Each node contains the key hash, a pointer
 * to the associated data node, and pointers to the previous and next nodes in the list.
 *
 * Fields:
 *   - key_hash: Hash of the key (immutable) - This is the sub-bucket hash of the key, taken from the composite_key_hash structure.
 *   - data_node_ptr: Pointer to the associated data_node.
 *   - prev_node_ptr: Pointer to the previous node in the list.
 *   - next_node_ptr: Pointer to the next node in the list.
 */
typedef struct double_linked_list_node
{
    uint64_t key_hash;
    data_node* data_node_ptr;
    struct double_linked_list_node* prev_node_ptr;
    struct double_linked_list_node* next_node_ptr;
} double_linked_list_node;

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

/**
 * @struct new_operation_buffer
 * @brief Represents a buffer for new operations during resizing.
 * The new_operation_buffer structure manages a doubly linked list of new operations
 * that occur while a hash bucket is being resized. It includes pointers to the head of the
 * list, the current consumer node, and a spinlock for thread-safe access.
 * Fields:
 *  - head_ptr: Pointer to the head of the doubly linked list of new operations.
 *  - current_consumer_ptr: Pointer to the current consumer node in the list.
 *  - buffer_lock: Spinlock for synchronizing access to the buffer.
 * - pause_chasing: Flag to indicate if chasing should be paused (used for synchronization during resizing).
 */
typedef struct 
{
    double_linked_list_node* head_ptr;
    double_linked_list_node* tail_ptr;
    double_linked_list_node* current_consumer_ptr;
    pthread_spinlock_t buffer_lock;
    bool pause_chasing;
    bool is_running;
}new_operation_buffer;

/**
 * @struct delete_operation
 * @brief Represents a delete operation for a key in the keystore.
 *
 * The delete_operation structure encapsulates the information needed to perform a delete operation,
 * including the key to be deleted and its associated hash. This structure is used in the context of
 * managing delete operations during resizing or cleanup processes.
 *
 * Fields:
 *   - key: Pointer to the null-terminated string representing the key to be deleted.
 *   - key_hash: Hash of the key (immutable) - This is the sub-bucket hash of the key, taken from the composite_key_hash structure.
 */
typedef struct{
    char* key;
    composite_key_hash key_hash;
} delete_operation;

/*
* @struct delete_operation_buffer
* @brief Represents a buffer for delete operations during resizing.
*
* The delete_operation_buffer structure manages a dynamic array of delete operations that need to be processed,
* particularly during resizing of hash buckets. It includes an array of delete operations, the count
* of operations currently in the buffer, and the capacity of the buffer to manage memory efficiently.
*
* Fields:
*   - operations: Pointer to an array of delete_operation structures representing the delete operations to be processed.
*   - count: The current number of delete operations stored in the buffer.
*   - capacity: The total capacity of the buffer, indicating how many delete operations it can hold before needing to resize.
*/
typedef struct{
    delete_operation* operations;
    unsigned int count;
    unsigned int capacity;
} delete_operation_buffer;


#endif // CUSTOM_TYPE_DEFINITIONS_H