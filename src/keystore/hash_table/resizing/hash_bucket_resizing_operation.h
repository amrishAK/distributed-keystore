#ifndef HASH_BUCKET_RESIZING_OPERATION_H
#define HASH_BUCKET_RESIZING_OPERATION_H

#include "type_definitions/hash_bucket_type_definition.h"
#include "type_definitions/error_code_definitions.h"
#include "type_definitions/sucess_code_definitions.h"

typedef enum{
    RESIZE_INITIALIZE,
    RESIZE_UPSERT_NODE,
    RESIZE_GET_NODE,
    RESIZE_DELETE_NODE,
    RESIZE_CHECK_STATUS
} hash_bucket_resizing_operation_t;

/** 
 * @fn initialize_hash_bucket_resizing
 * @brief Initializes the resizing process for a hash bucket.
 * @param hash_bucket_ptr Pointer to the hash bucket to be resized.
 * @return int Returns 0 on success, or a negative value on failure.
*/ 
int initialize_hash_bucket_resizing(hash_bucket* hash_bucket_ptr);

/** 
 * @fn upsert_node_to_hash_bucket_during_resizing
 * @brief Sets or updates a data node in the hash bucket during resizing by key and key hash.
 * @param hash_bucket_ptr Pointer to the hash bucket.
 * @param key_hash Hash of the key.
 * @param kv_pair Pointer to the key-value pair to be inserted or updated.
 * @return positive result on success, negative result if the node was not found or on error
 * @note The operation will be added to the pending list (for both update and insert)
 * @note This function should be called with the resizing lock already acquired, to ensure thread safety during resizing operations.
 * @note If resizing has completed while waiting for the lock, it will delegate to the normal upsert function.
*/
int upsert_node_to_hash_bucket_during_resizing(hash_bucket* hash_bucket_ptr, uint32_t key_hash, key_value_pair* kv_pair);

/** 
 * @fn get_key_value_from_hash_bucket_during_resizing
 * @brief Retrieves a data node's value from the hash bucket during resizing by key and key hash.
 * @param hash_bucket_ptr Pointer to the hash bucket.
 * @param key The key string.
 * @param key_hash Hash of the key.
 * @param kv_pair_out Pointer to a key_value_pair structure to receive the found value.
 * @return int Returns 0 on success, or a negative value if the node was not found.
 * @note This function should be called with the resizing lock already acquired, to ensure thread safety during resizing operations.
 * @note If resizing has completed while waiting for the lock, it will delegate to the normal get function.
*/
int get_key_value_from_hash_bucket_during_resizing(hash_bucket* hash_bucket_ptr, const char *key, uint32_t key_hash, key_value_pair* kv_pair_out);

/** 
 * @fn delete_key_from_hash_bucket_during_resizing
 * @brief Deletes a data node from the hash bucket during resizing by key and key hash.
 * @param hash_bucket_ptr Pointer to the hash bucket.
 * @param key The key string.
 * @param key_hash Hash of the key.
 * @return int Returns 0 on success, or a negative value if the node was not found.
 * @note The delete operation will be added to the pending list.
 * @note This function should be called with the resizing lock already acquired, to ensure thread safety during resizing operations.
 * @note If resizing has completed while waiting for the lock, it will delegate to the normal delete function.
*/
int delete_key_from_hash_bucket_during_resizing(hash_bucket* hash_bucket_ptr, const char *key, uint32_t key_hash);

/** 
 * @fn wait_for_ongoing_resize_to_complete
 * @brief Waits for any ongoing resizing operation on the hash bucket to complete.
 * @param hash_bucket_ptr Pointer to the hash bucket.
 * @return int Returns 0 on success, or a negative value on error.
 * @note This function should be called when an operation detects that resizing is in progress and needs to wait for it to finish before proceeding.
*/
int check_resize_status(hash_bucket* hash_bucket_ptr);

#endif // HASH_BUCKET_RESIZING_OPERATION_H