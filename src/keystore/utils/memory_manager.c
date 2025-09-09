#include "memory_manager.h"
#include "core/type_definition.h"
#include <math.h>

// global variables for memory pools
static memory_pool g_list_pool = {0};
static memory_pool g_tree_pool = {0};
static memory_manager_config g_config = {0};

// function declarations
static int _create_memory_pool(memory_pool *pool, size_t block_size);
static void* _allocate_memory_from_pool(memory_pool *memory_pool);
static void _free_memory_to_pool(memory_pool *memory_pool, void *ptr);
static bool _is_pointer_from_pool (memory_pool *pool, void *ptr);
static int _cleanup_memory_pool(memory_pool *pool);

int initialize_memory_manager(const memory_manager_config config)
{
    if(config.bucket_size == 0 || config.pre_allocation_factor <= 0 || config.pre_allocation_factor > 1)
    {
        return -1; // Invalid configuration
    }

    g_config = config;
    int pool_creation_result = 0;

    if(config.allocate_list_pool)
    {
        pool_creation_result = _create_memory_pool(&g_list_pool, sizeof(list_node));
    }

    if(config.allocate_tree_pool)
    {
        pool_creation_result = _create_memory_pool(&g_tree_pool, sizeof(tree_node));
    }

    if(pool_creation_result != 0)
    {
        cleanup_memory_manager();
        return -1; // Failed to create one of the pools
    }

    return pool_creation_result;
}

int cleanup_memory_manager(void)
{
    int result = 0;

    if(g_config.allocate_list_pool)
    {
        result = _cleanup_memory_pool(&g_list_pool);
    }

    if(g_config.allocate_tree_pool)
    {
        result = _cleanup_memory_pool(&g_tree_pool);
    }

    g_config = (memory_manager_config){0}; // Reset config
    return result;
}


void* allocate_memory_from_pool(memory_pool_type_t pool_type)
{
    switch(pool_type)
    {
        case LIST_POOL: return _allocate_memory_from_pool(&g_list_pool);
        case TREE_POOL: return _allocate_memory_from_pool(&g_tree_pool);
        default: return NULL; // Unsupported pool type
    }
}

void* allocate_memory(size_t size)
{
    return malloc(size);
}

void free_memory(void *ptr, memory_pool_type_t pool_type)
{
    switch(pool_type)
    {
        case LIST_POOL: _free_memory_to_pool(&g_list_pool, ptr); break;
        case TREE_POOL: _free_memory_to_pool(&g_tree_pool, ptr); break;
        default: free(ptr); break; // Use standard free for unsupported pool types
    }
}

void* reallocate_memory(void *ptr, size_t new_size)
{
    if (ptr == NULL)
    {
        return NULL; // Handle invalid parameters
    }

    if(new_size == 0)
    {
        free(ptr);
        return NULL; // Free memory if new size is zero
    }

    void *new_ptr = realloc(ptr, new_size);
    if (new_ptr == NULL)
    {
        return NULL; // Handle reallocation failure
    }
    return new_ptr;
}

/**
 * @fn _create_memory_pool
 * @brief Creates and initializes a memory pool with the specified block size.
 *
 * This function allocates memory for the pool based on the block size and
 * pre-allocation factor defined in the global configuration. It sets up
 * the necessary pointers and counters for managing the pool.
 *
 * @param pool Pointer to the memory_pool structure to be initialized.
 * @param block_size Size of each block in the pool.
 * @return 0 on success, or a negative error code on failure.
 */
int _create_memory_pool(memory_pool *pool, size_t block_size)
{
    if(pool == NULL || block_size == 0 || g_config.pre_allocation_factor < 0)
    {
        return -1; // Invalid parameters
    }

    if(g_config.pre_allocation_factor == 0)
    {
        return 0; // No pre-allocation requested
    }

    pool->block_size = block_size;
    pool->total_blocks = (int)ceil(g_config.bucket_size * g_config.pre_allocation_factor);
    pool->available_blocks = pool->total_blocks;
    pool->reusable_blocks = 0;
    pool->is_initialized = false;

    // Allocate memory for the pool
    pool->next_block_ptr = malloc(block_size * pool->total_blocks);
    if(pool->next_block_ptr == NULL)
    {
        return -1; // Memory allocation failed
    }

    // Allocate memory for the free block list
    pool->free_block_list = (void **)malloc(sizeof(void *) * pool->total_blocks);
    if(pool->free_block_list == NULL)
    {
        free(pool->next_block_ptr);
        pool->next_block_ptr = NULL;
        return -1; // Memory allocation failed
    }

    pool->pool_start_ptr = (char *)pool->next_block_ptr;
    pool->pool_end_ptr = (char *)pool->next_block_ptr + (pool->block_size * pool->total_blocks);
    pool->is_initialized = true;

    return 0; // Success
}


/**
 * @fn _allocate_memory_from_pool
 * @brief Allocates a memory block from the specified memory pool.
 *
 * This function checks if there are reusable blocks available in the pool.
 * If so, it returns one of those blocks. If not, it checks if there are
 * available blocks in the pre-allocated pool. If the pool is exhausted,
 * it falls back to standard malloc().
 *
 * @param memory_pool Pointer to the memory pool from which to allocate memory.
 * @return Pointer to the allocated memory block, or NULL if allocation fails.
 */
void* _allocate_memory_from_pool(memory_pool *memory_pool)
{
    void* mem = NULL;

    if(memory_pool->is_initialized)
    {
        if(memory_pool->reusable_blocks > 0)
        {
            mem = memory_pool->free_block_list[--memory_pool->reusable_blocks];
        }
        else
        {
            if(memory_pool->available_blocks > 0) {

                mem = memory_pool->next_block_ptr;
                memory_pool->next_block_ptr = (char*)memory_pool->next_block_ptr + memory_pool->block_size; // Move pointer to next block
                memory_pool->available_blocks--;
            }
            else
            {
                mem = malloc(memory_pool->block_size); // Fallback to standard malloc if pool is exhausted
            }
        }
    }

    return mem;
}

/**
 * @fn _free_memory_to_pool
 * @brief Frees a memory block back to the specified memory pool.
 *
 * This function checks if the pointer belongs to the memory pool and if there is space
 * in the reusable block list. If both conditions are met, it adds the block back to the
 * pool for reuse. Otherwise, it falls back to standard free().
 *
 * @param memory_pool Pointer to the memory pool to which the block should be returned.
 * @param ptr Pointer to the memory block to be freed.
 */
void _free_memory_to_pool(memory_pool *memory_pool, void *ptr)
{
    if(memory_pool->is_initialized)
    {
        if(_is_pointer_from_pool(memory_pool, ptr))
        {
           if(memory_pool->reusable_blocks < memory_pool->total_blocks)
            {
                memory_pool->free_block_list[memory_pool->reusable_blocks++] = ptr;
                return;
            }
        }
    }

    free(ptr); // Fallback to standard free if not from pool or pool is full
}


/**
 * @fn _is_pointer_from_pool
 * @brief Checks if a given pointer belongs to the specified memory pool.
 *
 * This function verifies whether the provided pointer falls within the memory range
 * allocated for the memory pool and aligns with the block size.
 *
 * @param pool Pointer to the memory pool to check against.
 * @param ptr Pointer to the memory to be checked.
 * @return true if the pointer belongs to the pool, false otherwise.
 */
bool _is_pointer_from_pool (memory_pool *pool, void *ptr) {
    if(pool == NULL || ptr == NULL) {
        return false;
    }
    
    if(ptr >= (void *)pool->pool_start_ptr && ptr < (void *)pool->pool_end_ptr) {
        size_t offset = (char *)ptr - pool->pool_start_ptr;
        return (offset % pool->block_size) == 0;
    }
    return false;
}

/**
 * @fn _cleanup_memory_pool
 * @brief Cleans up the specified memory pool.
 *
 * This function releases all resources associated with the memory pool,
 * including the pool memory and any free block lists.
 *
 * @param pool Pointer to the memory pool to clean up.
 * @return 0 on success, or a negative error code on failure.
 */
int _cleanup_memory_pool(memory_pool *pool)
{
    if(pool == NULL)
    {
        return -1; // Invalid parameter
    }

    if(!pool->is_initialized)
    {
        return 0; // Pool not initialized, nothing to clean
    }

    if(pool->pool_start_ptr != NULL)
    {
        free(pool->pool_start_ptr);
        pool->pool_start_ptr = NULL;
    }

    if(pool->free_block_list != NULL)
    {
        free(pool->free_block_list);
        pool->free_block_list = NULL;
    }

    pool->block_size = 0;
    pool->total_blocks = 0;
    pool->reusable_blocks = 0;
    pool->is_initialized = false;
    pool->available_blocks = 0;
    pool->next_block_ptr = NULL;
    pool->pool_end_ptr = NULL;

    return 0; // Success
}