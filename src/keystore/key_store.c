#include <stdlib.h>
#include <string.h>
#include "key_store.h"
#include "hash_buckets.h"
#include "hash_bucket_list.h"
#include "hash_functions.h"

// global variables
static hash_bucket *g_hash_buckets_ptr = NULL;
static int g_bucket_size = 0;
static int g_hash_seed = 0;

// function declarations
bool is_power_of_two(int n);
int get_bucket_index(uint32_t key_hash);
void set_list(hash_bucket *hash_bucket_ptr, const char *key, uint32_t key_hash, const unsigned char *data);


int initialise_key_store(int bucket_size) {
    if (!is_power_of_two(bucket_size)) {
        // Handle error: bucket_size must be a power of two
        return 1;
    }

    g_bucket_size = bucket_size;
    g_hash_buckets_ptr = malloc(sizeof(hash_bucket) * bucket_size);

    if (g_hash_buckets_ptr == NULL) {
        // Handle error: memory allocation failed
        return -1;
    }

    return 0;
}

int set_key(const char *key, const unsigned char *data) {
    uint32_t key_hash = hash_function_murmur_32(key, g_hash_seed);
    int index = get_bucket_index(key_hash);

    if (index == -1) {
        // Handle error: invalid index
        return -1;
    }

    hash_bucket *hash_bucket_ptr = get_hash_bucket(g_hash_buckets_ptr, index, true);

    switch (hash_bucket_ptr->type)
    {
        case BUCKET_LIST:
            set_list(hash_bucket_ptr, key, key_hash, data);
            break;
        default: return -1;
    }

    return 0;
}

data_node* get_key(const char *key) {
    // Retrieve value by key from the key store
    return NULL;
}

int delete_key(const char *key) {
    // Delete key-value pair from the key store
    return 0;
}



// Private function definitions

void set_list(hash_bucket *hash_bucket_ptr, const char *key, uint32_t key_hash, const unsigned char *data) {

    if (hash_bucket_ptr->type != BUCKET_LIST) {
        return;
    }

    list_node *header_ptr = (list_node *)hash_bucket_ptr->container;

    list_node *list_node_ptr = find_list_node(header_ptr, key, key_hash);

    if(list_node_ptr != NULL) {
        // Update existing data
        list_node_ptr->data->data = (unsigned char *)data;
        list_node_ptr->data->data_size = sizeof(data);
    } else {
        // Insert new data
        data_node *new_data_node = malloc(sizeof(data_node));
        
        if (new_data_node == NULL) {
            // Handle error: memory allocation failed
            return;
        }

        new_data_node->key = strdup(key);
        new_data_node->key_hash = key_hash;
        new_data_node->data = (unsigned char *)data;
        new_data_node->data_size = sizeof(data);

        hash_bucket_ptr->container = insert_list_node(header_ptr, key, key_hash, new_data_node);
    }
}



int get_bucket_index(uint32_t key_hash) {
    int index = key_hash & (g_bucket_size - 1);

    if(index < 0 || index >= g_bucket_size) {
        index = -1;
    }
    
    return index;
}


bool is_power_of_two(int n) {
    return n > 0 && (n & (n - 1)) == 0;
}
