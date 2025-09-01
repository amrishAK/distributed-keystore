#include <stdlib.h>
#include "hash_buckets.h"

//function decleration
hash_bucket* create_bucket(hash_bucket* hash_bucket_ptr, int index);
bool check_if_bucket_container_exists(hash_bucket *hash_bucket_ptr, int index);


hash_bucket* get_hash_bucket(hash_bucket *hash_bucket_ptr, int index, bool create_if_missing) {

    hash_bucket* target_bucket_ptr = NULL;

    if (check_if_bucket_container_exists(hash_bucket_ptr, index)) {
        target_bucket_ptr = &hash_bucket_ptr[index];
    } else if (create_if_missing) {
        target_bucket_ptr = create_bucket(hash_bucket_ptr, index);
    } else {
        return NULL;
    }

    return target_bucket_ptr;
}

void delete_hash_bucket(hash_bucket *hash_bucket_ptr, int index) {
    if (check_if_bucket_container_exists(hash_bucket_ptr, index)) {
        // Free the container and any associated resources

        switch (hash_bucket_ptr[index].type)
        {
        case BUCKET_LIST:
            free(hash_bucket_ptr[index].container.list);
            hash_bucket_ptr[index].container.list = NULL;
            break;
        case BUCKET_TREE:
            free(hash_bucket_ptr[index].container.tree);
            hash_bucket_ptr[index].container.tree = NULL;
            break;
        default:
            break;
        }

        hash_bucket_ptr[index].type = NONE;
        hash_bucket_ptr[index].count = 0;
    }
}

hash_bucket* create_bucket(hash_bucket* hash_bucket_ptr, int index)
{
    hash_bucket_ptr[index].type = BUCKET_LIST;
    // Initialize container to NULL; allocation should be handled elsewhere if needed
    hash_bucket_ptr[index].container.list = NULL;
    hash_bucket_ptr[index].count = 0;

    return &hash_bucket_ptr[index];
}

bool check_if_bucket_container_exists(hash_bucket *hash_bucket_ptr, int index) {

    switch (hash_bucket_ptr[index].type)
    {
    case BUCKET_LIST:
        return hash_bucket_ptr[index].container.list != NULL;
    case BUCKET_TREE:
        return hash_bucket_ptr[index].container.tree != NULL;
    default:
        return false;
    }
}

