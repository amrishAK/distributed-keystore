#ifndef BUFFER_OPERATION_H
#define BUFFER_OPERATION_H

#include "type_definitions/hash_bucket_type_definition.h"
#include "type_definitions/custom_type_definitions.h"
#include "type_definitions/error_code_definitions.h"
#include "type_definitions/sucess_code_definitions.h"



int chase_buffer_worker(void* input_arg);
int initialize_chase_worker(resizing_buffer* resizing_buffer_ptr, uint32_t* out_task_uuid);
int wait_for_chase_worker_to_finish(resizing_buffer* resizing_buffer_ptr, uint32_t task_uuid);

int initialize_resizing_buffer(hash_bucket* hash_bucket_ptr);
int insert_node_to_new_operation_buffer(hash_bucket* hash_bucket_ptr, uint32_t key_hash, key_value_pair* kv_pair, bool is_delete_operation);
int delete_resizing_buffer(resizing_buffer* resizing_buffer_ptr);

int insert_delete_operation_to_resizing_buffer(hash_bucket* hash_bucket_ptr, uint32_t key_hash, const char* key);
int insert_update_operation_to_resizing_buffer(hash_bucket* hash_bucket_ptr, uint32_t key_hash, key_value_pair* kv_pair);

int get_node_from_resizing_buffer(hash_bucket* hash_bucket_ptr, uint32_t key_hash, const char* key, bool ignore_current_operation_buffer, key_value_pair* kv_pair_out);

int commit_resizing_buffer_operations_to_sub_hash_table(hash_bucket* hash_bucket_ptr);


#endif // BUFFER_OPERATION_H