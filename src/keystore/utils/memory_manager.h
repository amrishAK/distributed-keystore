/**
 * @file memory_manager.h
 * @brief Memory management utilities for the KeyStore.
 *
 * This header provides types and functions for managing memory pools and
 * general memory allocation in the KeyStore project.
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
#include <pthread.h>
#include "type_definitions/error_code_definitions.h"
#include "type_definitions/sucess_code_definitions.h"
#include "type_definitions/config_type_definitions.h"
#include "type_definitions/custom_type_definitions.h"


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
 * @fn allocate_memory_from_pool
 * @brief Allocates a block of memory from the linked list memory pool.
 *
 * This function attempts to allocate a memory block from the designated memory pool.
 * If the pool is exhausted, it falls back to standard malloc().
 *
 * @return Pointer to the allocated memory block, or NULL if allocation fails.
 */
void* allocate_memory_from_pool();

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
 * @fn callocate_memory
 * @brief Allocates and zero-initializes an array of memory blocks.
 * @param num Number of elements to allocate.
 * @param size Size of each element in bytes.
 * @return A pointer to the allocated memory block, or NULL if allocation fails.
 * @note - It uses standard calloc() internally.
 */
void* callocate_memory(size_t num, size_t size);

/**
 * @fn reallocate_memory
 * @brief Reallocates a memory block to a new size.
 * @param ptr Pointer to the existing memory block to be reallocated.
 * @param new_size The new size in bytes for the memory block.
 * @return A pointer to the reallocated memory block, or NULL if reallocation fails.
 * @note - It uses standard realloc() internally.
 */
void* reallocate_memory(void *ptr, size_t new_size);

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
 * @param is_pool Indicates whether the memory block was allocated from a pool.
 */
void free_memory(void* ptr, bool is_pool);

#endif // MEMORY_MANAGER_H