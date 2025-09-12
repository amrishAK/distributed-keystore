#ifndef TYPE_DEFINITION_H
#define TYPE_DEFINITION_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <pthread.h>

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

    int count;
    bool is_initialized;
    pthread_rwlock_t lock;
} hash_bucket;

#endif // TYPE_DEFINITION_H