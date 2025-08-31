#ifndef HASH_BUCKETS_H
#define HASH_BUCKETS_H

#include "type_definition.h"
#include <stdbool.h>


/// @brief Get a hash bucket by index, creating it if it doesn't exist.
/// @param hash_bucket_ptr Pointer to the hash bucket array.
/// @param index Index of the desired bucket.
/// @param create_if_missing Whether to create the bucket if it doesn't exist.
/// @return Pointer to the hash bucket, or NULL if it couldn't be found/created.
hash_bucket* get_hash_bucket(hash_bucket *hash_bucket_ptr, int index, bool create_if_missing);
void delete_hash_bucket(hash_bucket *hash_bucket_ptr, int index);




#endif // HASH_BUCKETS_H