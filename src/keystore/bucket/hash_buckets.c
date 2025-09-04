#include <stdlib.h>
#include "hash_buckets.h"
#include "hash_bucket_list.h"

//function decleration
hash_bucket* create_bucket(hash_bucket* hash_bucket_ptr, int index);


hash_bucket*  get_hash_bucket(hash_bucket *hash_bucket_ptr, int index, bool create_if_missing) {

    hash_bucket* target_bucket_ptr = NULL;
    
    if (hash_bucket_ptr != NULL && index >= 0) {
        target_bucket_ptr = &hash_bucket_ptr[index];
    }
    else {
        return NULL; // Invalid parameters
    }

    if(!check_if_bucket_container_exists(target_bucket_ptr))
    {
        if(create_if_missing) 
        {
            target_bucket_ptr = create_bucket(hash_bucket_ptr, index);
        }
        else 
        {
            target_bucket_ptr = NULL; // Set pointer to NULL since bucket does not exist and creation is not allowed
        }
    }

    return target_bucket_ptr;
}

void delete_hash_bucket(hash_bucket *hash_bucket_ptr) {
    if (check_if_bucket_container_exists(hash_bucket_ptr)) {
        // Free the container and any associated resources

        switch (hash_bucket_ptr->type)
        {
        case BUCKET_LIST:
            delete_all_list_nodes(hash_bucket_ptr->container.list);
            hash_bucket_ptr->container.list = NULL;
        default:
            break;
        }

        hash_bucket_ptr->type = NONE;
        hash_bucket_ptr->count = 0;
        free(hash_bucket_ptr);
    }
}

int add_node_to_bucket(hash_bucket *hash_bucket_ptr, uint32_t key_hash, data_node *data_node_ptr)
{
    switch (hash_bucket_ptr->type)
    {
        case BUCKET_LIST:
            return insert_list_node(&hash_bucket_ptr->container.list, key_hash, data_node_ptr);
        default:
            return -1; // Unsupported bucket type
    }

    return 0;
}

data_node *find_node_in_bucket(hash_bucket *hash_bucket_ptr, const char *key, uint32_t key_hash)
{
    if (hash_bucket_ptr == NULL) {
        return NULL; // Handle error: invalid bucket
    }

    switch (hash_bucket_ptr->type)
    {
        case BUCKET_LIST: 
            list_node *found_node = find_list_node(hash_bucket_ptr->container.list, key, key_hash);
            return found_node != NULL ? found_node->data : NULL;
        default: return NULL; // Unsupported bucket type
    }
}

int delete_node_from_bucket(hash_bucket *hash_bucket_ptr, const char *key, uint32_t key_hash)
{
    if (hash_bucket_ptr == NULL) {
        return -1; // Handle error: invalid bucket
    }

    switch (hash_bucket_ptr->type)
    {
        case BUCKET_LIST: 
            return delete_list_node(&hash_bucket_ptr->container.list, key, key_hash);
        default: return -1; // Unsupported bucket type
    }
}

hash_bucket* create_bucket(hash_bucket* hash_bucket_ptr, int index)
{
    hash_bucket_ptr[index].type = BUCKET_LIST;
    hash_bucket_ptr[index].container.list = NULL;
    hash_bucket_ptr[index].count = 0;

    return &hash_bucket_ptr[index];
}

bool check_if_bucket_container_exists(hash_bucket *hash_bucket_ptr) {

    switch (hash_bucket_ptr->type)
    {
    case BUCKET_LIST:
        return hash_bucket_ptr->container.list != NULL;
    case BUCKET_TREE:
        return hash_bucket_ptr->container.tree != NULL;
    default:
        return false;
    }
}

