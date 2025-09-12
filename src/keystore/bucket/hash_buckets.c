#include <stdlib.h>
#include "hash_buckets.h"
#include "hash_bucket_list.h"
#include "core/type_definition.h"
#include "utils/memory_manager.h"

//global variables
static hash_bucket_memory_pool g_hash_bucket_pool = {0};

// struct
typedef struct bucket_operation_args {
    hash_bucket *bucket;
    uint32_t key_hash;
    const char *key; // For delete_node and find_node
    data_node *node; // For add_node
} bucket_operation_args;

typedef enum {
    ADD_NODE,
    DELETE_NODE,
    FIND_NODE
} bucket_operation_type_t;

//function decleration
static bool _is_power_of_two(unsigned int n);
static void _initialise_hash_bucket(hash_bucket *hash_bucket_ptr);
static void _delete_hash_bucket(unsigned int index);
static int _add_node(bucket_operation_args args);
static int _delete_node(bucket_operation_args args);
static data_node* _find_node(bucket_operation_args args);

static int _lock_wrapper(bucket_operation_type_t operation_type, bucket_operation_args args, data_node** result_out);



int initialise_hash_buckets(unsigned int bucket_size, bool is_concurrency_enabled) 
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
    g_hash_bucket_pool.is_concurrency_enabled = is_concurrency_enabled;

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

    bucket_operation_args input_args = {hash_bucket_ptr, key_hash, NULL, data_node_ptr};

    return  g_hash_bucket_pool.is_concurrency_enabled ? _lock_wrapper(ADD_NODE, input_args , NULL) : _add_node(input_args);
}

data_node *find_node_in_bucket(unsigned int index, const char *key, uint32_t key_hash)
{
    hash_bucket *hash_bucket_ptr = get_hash_bucket(index, false);

    if (hash_bucket_ptr == NULL) {
        return NULL; // Handle error: invalid bucket
    }

    data_node *result = NULL;
    bucket_operation_args input_args = {hash_bucket_ptr, key_hash, key, NULL};

    if(g_hash_bucket_pool.is_concurrency_enabled) {
        _lock_wrapper(FIND_NODE, input_args, &result);
    } else {
        result = _find_node(input_args);
    }
    return result;
}

int delete_node_from_bucket(unsigned int index, const char *key, uint32_t key_hash)
{
    hash_bucket *hash_bucket_ptr = get_hash_bucket(index, false);

    if (hash_bucket_ptr == NULL) {
        return -1; // Handle error: invalid bucket
    }

    bucket_operation_args input_args = {hash_bucket_ptr, key_hash, key, NULL};
    return g_hash_bucket_pool.is_concurrency_enabled ? _lock_wrapper(DELETE_NODE, input_args, NULL) : _delete_node(input_args);
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


/**
 * @fn _initialise_hash_bucket
 * @brief Initializes a hash bucket to its default state.
 *
 * This function sets the type of the hash bucket to BUCKET_LIST,
 * initializes its container to NULL, sets the count to 0, marks it
 * as initialized, and initializes the read-write lock for concurrency control.
 *
 * @param hash_bucket_ptr Pointer to the hash_bucket structure to be initialized.
 */
void _initialise_hash_bucket(hash_bucket *hash_bucket_ptr) {
    hash_bucket_ptr->type = BUCKET_LIST;
    hash_bucket_ptr->container.list = NULL;
    hash_bucket_ptr->count = 0;
    hash_bucket_ptr->is_initialized = true;

    if (g_hash_bucket_pool.is_concurrency_enabled) {
        pthread_rwlock_init(&hash_bucket_ptr->lock, NULL);
    }
}

/**
 * @fn _delete_hash_bucket
 * @brief Cleans up and resets a hash bucket at the specified index.
 *
 * This function deletes all nodes in the hash bucket, frees associated memory,
 * and resets the bucket's properties to indicate it is uninitialized.
 *
 * @param index The index of the hash bucket to be deleted.
 */
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

/**
 * @fn _add_node
 * @brief Adds a data node to the specified hash bucket.
 *
 * This function inserts a data node into the hash bucket's container
 * based on its type (currently only BUCKET_LIST is supported).
 *
 * @param args A struct containing the hash bucket, key hash, and data node to be added.
 * @return int Returns 0 on success, or -1 on failure.
 */
int _add_node(bucket_operation_args args)
{
    switch (args.bucket->type)
    {
        case BUCKET_LIST:
            return insert_list_node(&args.bucket->container.list, args.key_hash, args.node);
        default:
            return -1; // Unsupported bucket type
    }
}

/**
 * @fn _delete_node
 * @brief Deletes a data node from the specified hash bucket.
 *
 * This function removes a data node from the hash bucket's container
 * based on its type (currently only BUCKET_LIST is supported).
 *
 * @param args A struct containing the hash bucket, key hash, and key of the node to be deleted.
 * @return int Returns 0 on success, or -1 on failure.
 */
int _delete_node(bucket_operation_args args)
{
    switch (args.bucket->type)
    {
        case BUCKET_LIST:
            return delete_list_node(&args.bucket->container.list, args.key, args.key_hash);
        default:
            return -1; // Unsupported bucket type
    }
}

/**
 * @fn _find_node
 * @brief Finds a data node in the specified hash bucket.
 *
 * This function searches for a data node in the hash bucket's container
 * based on its type (currently only BUCKET_LIST is supported).
 *
 * @param args A struct containing the hash bucket, key hash, and key of the node to be found.
 * @return data_node* Pointer to the found data node, or NULL if not found.
 */
data_node* _find_node(bucket_operation_args args)
{
    switch (args.bucket->type)
    {
        case BUCKET_LIST:
            list_node* found_node = find_list_node(args.bucket->container.list, args.key, args.key_hash);
            return (found_node != NULL) ? found_node->data : NULL;
        default:
            return NULL; // Unsupported bucket type
    }
}

/**
 * @fn _lock_wrapper
 * @brief Wrapper function to handle locking for bucket operations.
 *
 * This function acquires the appropriate lock (read or write) based on the
 * operation type, performs the operation, and then releases the lock.
 *
 * @param operation_type The type of bucket operation (ADD_NODE, DELETE_NODE, FIND_NODE).
 * @param args A struct containing the necessary arguments for the operation.
 * @param result_out Pointer to a data_node pointer to store the result (only used for FIND_NODE, pass NULL for other operations).
 * @return int Returns 0 on success, or -1 on failure.
 */
int _lock_wrapper(bucket_operation_type_t operation_type, bucket_operation_args args, data_node** result_out) {
    int lock_result = 0;
    if (operation_type == FIND_NODE) {
        lock_result = pthread_rwlock_rdlock(&args.bucket->lock);
    } else {
        lock_result = pthread_rwlock_wrlock(&args.bucket->lock);
    }

    if (lock_result != 0) {
        return -1; // Failed to acquire lock
    }

    int operation_result = 0;
    switch (operation_type) {
        case ADD_NODE:
            operation_result = _add_node(args);
            break;
        case DELETE_NODE:
            operation_result = _delete_node(args);
            break;
        case FIND_NODE:
            *(data_node **)result_out = _find_node(args);
            operation_result = (*(data_node **)result_out != NULL) ? 0 : -1;
            break;
        default:
            operation_result = -1; // Unknown operation
            break;
    }

    pthread_rwlock_unlock(&args.bucket->lock);
    return operation_result;
}