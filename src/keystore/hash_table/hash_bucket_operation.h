#ifndef HASH_BUCKET_OPERATION_H
#define HASH_BUCKET_OPERATION_H

#include "type_definitions/hash_bucket_type_definition.h"

int initialise_hash_bucket(hash_bucket* hash_bucket_ptr, sub_hash_table_configuration sub_hash_table_config);

int cleanup_hash_bucket(hash_bucket* hash_bucket_ptr);

int upsert_node_to_hash_bucket(hash_bucket* hash_bucket_ptr, uint32_t key_hash, key_value_pair* kv_pair);

int get_key_value_from_hash_bucket(hash_bucket* hash_bucket_ptr, const char *key, uint32_t key_hash, key_value_pair* kv_pair_out);

int delete_key_from_hash_bucket(hash_bucket* hash_bucket_ptr, const char *key, uint32_t key_hash);

#endif // HASH_BUCKET_OPERATION_H