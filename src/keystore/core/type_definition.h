#ifndef TYPE_DEFINITION_H
#define TYPE_DEFINITION_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <pthread.h>


#pragma region Data Structures Type Definitions
typedef struct {
    unsigned char *data;
    size_t data_size;
} key_store_value;


typedef enum {
    NONE,
    BUCKET_LIST,
    BUCKET_TREE
} bucket_type_t;

typedef enum{
    RED,
    BLACK
} rb_tree_color_t;

typedef struct  data_node
{
    uint32_t key_hash; // Hash of the key (immutable)
    unsigned char *data;
    size_t data_size;
    bool is_concurrency_enabled;
    pthread_mutex_t lock; // Mutex for concurrency control
    char key[];
} data_node;

typedef struct list_node
{
    uint32_t key_hash; // Hash of the key (immutable)
    data_node *data;
    struct list_node *next;
} list_node;

typedef struct  tree_node
{
    rb_tree_color_t color;
    uint32_t key_hash; // Hash of the key (immutable)
    data_node *data;
    struct tree_node *left;
    struct tree_node *right;
    struct tree_node *parent;

} tree_node;

typedef struct  hash_bucket
{
    bucket_type_t type;
    union {
        list_node *list;
        tree_node *tree;
    } container;

    unsigned int count;
    bool is_initialized;
    pthread_rwlock_t lock;
} hash_bucket;

#pragma endregion

#pragma region Keystore Statistics Type Definition

typedef struct
{
    size_t init_timestamp;
    size_t last_cleanup_timestamp;
} metadata_stats;

typedef struct
{
    unsigned int total_keys;
    unsigned int total_buckets;
    unsigned int nonempty_buckets;
    unsigned int empty_buckets;
    unsigned int max_keys_in_bucket;
    unsigned int min_keys_in_bucket;
    double avg_keys_per_nonempty_bucket;
    double stddev_keys_per_bucket;
    double median_keys_per_bucket;
    double avg_collisions_per_nonempty_bucket;
    double empty_bucket_percent;
} key_entry_stats;

typedef struct
{
    unsigned int collision_buckets;
    double collision_percent;
    unsigned int highest_collision_in_bucket;
    double avg_collisions_per_nonempty_bucket;
} key_collision_stats;

typedef struct
{
    size_t total_memory_bytes;
    size_t used_memory_bytes;
    size_t free_memory_bytes;
    double memory_utilization_percent;
    size_t memory_per_key_bytes;
    double fragmentation_percent;
} memory_pool_stats;

typedef struct
{
    unsigned long total_add_ops;
    unsigned long total_edit_ops;
    unsigned long total_get_ops;
    unsigned long total_delete_ops;
    unsigned long failed_add_ops;
    unsigned long failed_edit_ops;
    unsigned long failed_get_ops;
    unsigned long failed_delete_ops;
    unsigned long error_code_counters[100]; // Array to hold counts for different error codes
} operation_counter_stats;

typedef struct {
    metadata_stats metadata;
    key_entry_stats key_entries;
    key_collision_stats collisions;
    memory_pool_stats memory_pool;
    operation_counter_stats operation_counters;
} keystore_stats;

#pragma endregion

#pragma region Memory Pool Type Definitions
typedef struct hash_bucket_memory_pool
{
    hash_bucket* hash_buckets_ptr; // Pointer to the array of hash buckets
    unsigned int block_size; // Size of each block
    unsigned int total_blocks; // Total number of blocks in the pool
    bool is_initialized; // Flag to indicate if the pool is initialized
    bool is_concurrency_enabled; // Flag to indicate if concurrency control is enabled
} hash_bucket_memory_pool;

#pragma endregion

#endif // TYPE_DEFINITION_H