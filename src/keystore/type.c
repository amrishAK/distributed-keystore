#include <stddef.h>

typedef enum {
    BUCKET_LIST,
    BUCKET_TREE
} bucket_type_t;

typedef struct  data_node
{
    char *key;
    char *key_hash;
    unsigned char *data;
    size_t data_size;
} data_node;

typedef struct list_node
{
    char *key;
    char *key_hash;
    data_node *data;
    struct list_node *next;
} list_node;

typedef struct  tree_node
{
    char *key;
    char *key_hash;
    data_node *data;
    struct tree_node *left;
    struct tree_node *right;
} tree_node;

typedef struct  hash_bucket
{
    bucket_type_t type;
    void *container; // points to list_node or tree_node
    int count;
} hash_bucket;