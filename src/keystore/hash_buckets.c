#include <stdlib.h>
#include "hash_buckets.h"

hash_bucket* create_bucket(hash_bucket* hash_bucket_ptr, int index);



hash_bucket* get_hash_bucket(hash_bucket *hash_bucket_ptr, int index, bool create_if_missing) {

    hash_bucket* target_bucket_ptr = NULL;

    if (hash_bucket_ptr[index].container != NULL) {
        target_bucket_ptr = &hash_bucket_ptr[index];
    } else if (create_if_missing) {
        target_bucket_ptr = create_bucket(hash_bucket_ptr, index);
    } else {
        return NULL;
    }

    return target_bucket_ptr;
}

void delete_hash_bucket(hash_bucket *hash_bucket_ptr, int index) {
    if (hash_bucket_ptr[index].container != NULL) {
        // Free the container and any associated resources
        free(hash_bucket_ptr[index].container);
        hash_bucket_ptr[index].container = NULL;
        hash_bucket_ptr[index].type = NONE;
        hash_bucket_ptr[index].count = 0;
    }
}

hash_bucket* create_bucket(hash_bucket* hash_bucket_ptr, int index)
{
    hash_bucket_ptr[index].type = BUCKET_LIST;
    // Initialize container to NULL; allocation should be handled elsewhere if needed
    hash_bucket_ptr[index].container = NULL;
    hash_bucket_ptr[index].count = 0;

    return &hash_bucket_ptr[index];
}
