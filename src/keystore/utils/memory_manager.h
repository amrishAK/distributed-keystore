/**
 * @file memory_manager.h
 * @brief Memory management utilities for distributed keystore.
 *
 * This header provides types and functions for managing memory pools and
 * general memory allocation in the distributed keystore project.
 *
 * Types:
 * - memory_pool_type_t: Enum for memory pool selection (NONE, LIST_POOL, TREE_POOL).
 * - memory_pool: Structure for memory pool block management.
 * - memory_manager_config: Configuration for memory manager initialization.
 *
 * Functions:
 * - initialize_memory_manager: Sets up memory pools based on configuration.
 * - cleanup_memory_manager: Releases all memory manager resources.
 * - allocate_memory_from_pool: Allocates a block from a specified pool.
 * - allocate_memory: Allocates memory from the heap.
 * - free_memory: Frees memory, returning it to the pool if applicable.
 *
 * Usage:
 * 1. Initialize the memory manager with memory_manager_config.
 * 2. Allocate and free memory using the provided functions.
 * 3. Clean up resources before program exit.
 */


#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include <stdlib.h>
#include <stdbool.h>


/**
 * @enum memory_pool_type_t
 * @brief Represents the type of memory pool used in the memory manager.
 *
 * This enumeration defines the available memory pool types:
 * @note - LIST_POOL: A memory pool based on a list structure.
 * @note - TREE_POOL: A memory pool based on a tree structure.
 */
typedef enum memory_pool_type_t {
    NO_POOL,
    LIST_POOL,
    TREE_POOL
} memory_pool_type_t;


/**
 * @struct memory_pool
 * @brief Structure representing a memory pool for efficient memory management.
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

} memory_pool;

typedef struct memory_manager_config {
    unsigned int bucket_size;
    double pre_allocation_factor;
    bool allocate_list_pool;
    bool allocate_tree_pool;
} memory_manager_config;

/**
 * @fn initialize_memory_manager
 * @brief Initializes the memory manager with the specified configuration.
 *
 * This function sets up the memory pools based on the provided configuration,
 * allocating necessary resources for efficient memory management.
 *
 * @param config The memory_manager_config structure containing initialization parameters.
 * @return 0 on success, or a negative error code on failure.
 */
int initialize_memory_manager(const memory_manager_config config);


/**
 * @fn cleanup_memory_manager
 * @brief Cleans up and releases all resources used by the memory manager.
 *
 * This function frees any memory and resources allocated for the memory pools,
 * ensuring no memory leaks occur. It should be called before program termination
 * to properly release all memory managed by the memory manager.
 *
 * @return 0 on success, or a negative error code on failure.
 */
int cleanup_memory_manager(void);


/**
 * @brief Allocates a memory block from the specified memory pool.
 *
 * This function obtains a memory block from the given pool type, which helps optimize
 * allocation for commonly used data structures. The memory pool mechanism can improve
 * performance and reduce fragmentation for frequent allocations.
 *
 * @note - If the specified pool type is unsupported, the function returns NULL.
 * @note - If the pool is exhausted, it falls back to standard malloc.
 * @note - The caller is responsible for freeing the allocated memory using free_memory().
 * @note - Ensure that the memory manager is initialized before calling this function.
 * @param pool_type The type of memory pool to allocate from (e.g., LIST_POOL, TREE_POOL).
 * @return A pointer to the allocated memory block, or NULL if allocation fails.
 */
void* allocate_memory_from_pool(memory_pool_type_t pool_type);

/**
 * @brief Allocates a block of memory of the given size.
 *
 * This function provides a wrapper around the standard malloc function,
 * allowing for consistent memory allocation throughout the keystore utility.
 *
 * @param size Number of bytes to allocate.
 * @return Pointer to the allocated memory block, or NULL if allocation fails.
 */
void* allocate_memory(size_t size);

/**
 * @fn free_memory
 * @brief Frees the specified memory block.
 *
 * This function wraps the standard free function to provide a consistent
 * interface for memory deallocation. If the memory block was allocated from a
 * memory pool, it returns the block to the appropriate pool for reuse.
 * Otherwise, it releases the memory back to the heap.
 *
 * @note - If the pool_type is unsupported, it uses standard free().
 * @note If the reused block list in the pool is full, it falls back to standard free().
 * @note - it uses standard free() if the pointer is not from the pool.
 * @param ptr Pointer to the memory block to free.
 * @param pool_type The type of memory pool the block was allocated from (LIST_POOL, TREE_POOL).
 */
void free_memory(void* ptr, memory_pool_type_t pool_type);

#endif // MEMORY_MANAGER_H