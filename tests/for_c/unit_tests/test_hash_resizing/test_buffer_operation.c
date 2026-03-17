/**
 * @file test_buffer_operation.c
 * @brief Unit tests for buffer operations during hash table resizing.
 *
 * Test Scenarios Covered:
 *   - test_upsert_node_to_resizing_buffer_null_args: Checks error handling for NULL and invalid arguments to upsert_node_to_resizing_buffer.
 *   - test_upsert_and_get_node_from_resizing_buffer: Inserts a node and retrieves it, verifying correct storage and retrieval.
 *   - test_insert_delete_operation_to_resizing_buffer_and_get: Inserts a delete operation and verifies retrieval marks the node as deleted.
 *   - test_insert_update_operation_to_resizing_buffer_and_get: Inserts an update operation and verifies retrieval of the updated value.
 *   - test_get_node_from_resizing_buffer_not_found: Attempts to retrieve a non-existent key, expecting a not found error.
 *   - test_delete_resizing_buffer_null: Checks error handling for NULL argument to delete_resizing_buffer.
 *   - test_delete_resizing_buffer_valid: Inserts a node, deletes the buffer, and checks for successful cleanup.
 *   - test_upsert_duplicate_key_and_get_latest: Inserts the same key twice and verifies the latest value is returned.
 *   - test_upsert_with_value_size_zero: Upserts a key with value_size=0, marking it as deleted, and verifies retrieval.
 *   - test_insert_update_with_null_value: Attempts to update with NULL value and size 0, expecting an error.
 *   - test_delete_and_reinsert_key: Deletes a key, then reinserts it, and verifies the latest value is returned.
 *   - test_sequence_of_operations: Performs a sequence of insert, update, delete, and get operations on the same key.
 *   - test_delete_resizing_buffer_on_empty: Deletes the buffer when it is already empty, expecting a success code.
 *
 * These tests ensure correctness, error handling, and robustness of buffer operations during hash table resizing.
 */

#include "unity.h"
#include "hash_table/resizing/buffer_operation.h"
#include "utils/memory_manager.h"
#include "sub_hash_table/sub_hash_table_operation.h"
#include "hash_table/hash_bucket_operation.h"
#include <string.h>
#include <stdio.h>

// Helper: create a minimal hash_bucket with resizing_buffer using new API
static void setup_minimal_hash_bucket(hash_bucket **bucket_out) {
	hash_bucket* bucket = callocate_memory(1, sizeof(hash_bucket));
	bucket->sub_hash_table_config.is_concurrency_enabled = false;
	initialize_resizing_buffer(bucket);
	*bucket_out = bucket;
}

static void cleanup_hash_bucket_and_buffer(hash_bucket* bucket) {
	if (bucket->resizing_buffer_ptr != NULL) {
		delete_resizing_buffer(bucket->resizing_buffer_ptr);
		bucket->resizing_buffer_ptr = NULL;
	}
	cleanup_hash_bucket(bucket);
	free_memory(bucket, false);
}

static void cleanup_key_value_pair(key_value_pair* kv, bool free_key_value) {
	if (kv) {
		if (free_key_value) {
			if (kv->key) free_memory(kv->key, false);
			if (kv->value) free_memory(kv->value, false);
		}
		free_memory(kv, false);
	}
}

void test_upsert_node_to_resizing_buffer_null_args(void) {
    TEST_ASSERT_EQUAL(ERR_INVALID_ARGUMENT, insert_node_to_new_operation_buffer(NULL, 1, NULL, false));
    hash_bucket* bucket;
	setup_minimal_hash_bucket(&bucket);
    TEST_ASSERT_EQUAL(ERR_INVALID_ARGUMENT, insert_node_to_new_operation_buffer(bucket, 0, NULL, false));
	cleanup_hash_bucket_and_buffer(bucket);
}

void test_upsert_and_get_node_from_resizing_buffer(void) {
    hash_bucket* bucket; 
	setup_minimal_hash_bucket(&bucket);
    key_value_pair kv = {"key1", (unsigned char*)"val1", 5};
    int result = insert_node_to_new_operation_buffer(bucket, 123, &kv, false);
    TEST_ASSERT_EQUAL(SUCCESS_ADDED_TO_PENDING_LIST, result);

	key_value_pair* out = callocate_memory(1, sizeof(key_value_pair));
	result = get_node_from_resizing_buffer(bucket, 123, "key1", false, out);
	TEST_ASSERT_EQUAL(SUCCESS, result);
	TEST_ASSERT_EQUAL_STRING("key1", out->key);
	TEST_ASSERT_EQUAL_STRING("val1", out->value);
	cleanup_hash_bucket_and_buffer(bucket);
	cleanup_key_value_pair(out, false);
}

void test_insert_delete_operation_to_resizing_buffer_and_get(void) {
	hash_bucket* bucket; 
    setup_minimal_hash_bucket(&bucket);
	
    int result = insert_delete_operation_to_resizing_buffer(bucket, 456, "key2");
	TEST_ASSERT_EQUAL(SUCCESS, result);

	key_value_pair* out = callocate_memory(1, sizeof(key_value_pair));
	result = get_node_from_resizing_buffer(bucket, 456, "key2", true, out);
	TEST_ASSERT_EQUAL(ERR_DATA_NODE_NOT_FOUND, result);
	cleanup_hash_bucket_and_buffer(bucket);
	cleanup_key_value_pair(out, false);
}

void test_insert_update_operation_to_resizing_buffer_and_get(void) {
	hash_bucket* bucket; 
	setup_minimal_hash_bucket(&bucket);
	key_value_pair kv = {"key3", (unsigned char*)"val3", 5};
	int result = insert_update_operation_to_resizing_buffer(bucket, 789, &kv);
	TEST_ASSERT_EQUAL(SUCCESS, result);

	key_value_pair* out = callocate_memory(1, sizeof(key_value_pair));
	result = get_node_from_resizing_buffer(bucket, 789, "key3", true, out);
	TEST_ASSERT_EQUAL(SUCCESS, result);
	TEST_ASSERT_EQUAL_STRING("key3", out->key);
	TEST_ASSERT_EQUAL_STRING("val3", out->value);
	cleanup_hash_bucket_and_buffer(bucket);
	cleanup_key_value_pair(out, false);
}

void test_get_node_from_resizing_buffer_not_found(void) {
	hash_bucket* bucket; 
	setup_minimal_hash_bucket(&bucket);
	key_value_pair* out = callocate_memory(1, sizeof(key_value_pair));
	int result = get_node_from_resizing_buffer(bucket, 999, "notfound", false, out);
	TEST_ASSERT_EQUAL(ERR_DATA_NODE_NOT_FOUND, result);
	cleanup_hash_bucket_and_buffer(bucket);
	cleanup_key_value_pair(out, false);
}

void test_delete_resizing_buffer_null(void) {
	TEST_ASSERT_EQUAL(ERR_INVALID_ARGUMENT, delete_resizing_buffer(NULL));
}

void test_delete_resizing_buffer_valid(void) {
	hash_bucket* bucket; 
	setup_minimal_hash_bucket(&bucket);
	key_value_pair kv = {"key4", (unsigned char*)"val4", 5};
	insert_node_to_new_operation_buffer(bucket, 321, &kv, false);
	int result = delete_resizing_buffer(bucket->resizing_buffer_ptr);
	TEST_ASSERT_EQUAL(SUCCESS, result);
	bucket->resizing_buffer_ptr = NULL;
	cleanup_hash_bucket_and_buffer(bucket);
}

// Insert duplicate key and ensure latest is returned
void test_upsert_duplicate_key_and_get_latest(void) {
    hash_bucket* bucket; 
	setup_minimal_hash_bucket(&bucket);
    key_value_pair kv1 = {"dupkey", (unsigned char*)"val1", 5};
    key_value_pair kv2 = {"dupkey", (unsigned char*)"val2", 5};
    insert_node_to_new_operation_buffer(bucket, 111, &kv1, false);
    insert_node_to_new_operation_buffer(bucket, 111, &kv2, false);
	key_value_pair* out = callocate_memory(1, sizeof(key_value_pair));
	int result = get_node_from_resizing_buffer(bucket, 111, "dupkey", false, out);
	TEST_ASSERT_EQUAL(SUCCESS, result);
	TEST_ASSERT_EQUAL_STRING("val2", out->value);
	cleanup_hash_bucket_and_buffer(bucket);
	cleanup_key_value_pair(out, false);
}

// Upsert with value_size = 0 should mark as deleted
void test_upsert_with_value_size_zero(void) {
    hash_bucket* bucket; 
	setup_minimal_hash_bucket(&bucket);
    key_value_pair kv = {"keydel", (unsigned char*)"", 0};
    insert_node_to_new_operation_buffer(bucket, 222, &kv, false);
	key_value_pair* out = callocate_memory(1, sizeof(key_value_pair));
	int result = get_node_from_resizing_buffer(bucket, 222, "keydel", false, out);
	TEST_ASSERT_EQUAL(SUCCESS, result);
	TEST_ASSERT_EQUAL(0, out->value_size);
	cleanup_hash_bucket_and_buffer(bucket);
	cleanup_key_value_pair(out, false);
}

// Insert update with NULL value or size 0 should error
void test_insert_update_with_null_value(void) {
    hash_bucket* bucket; 
	setup_minimal_hash_bucket(&bucket);
    key_value_pair kv = {"keyupd", NULL, 0};
    int result = insert_update_operation_to_resizing_buffer(bucket, 333, &kv);
    TEST_ASSERT_EQUAL(ERR_INVALID_ARGUMENT, result);
    cleanup_hash_bucket_and_buffer(bucket);
}

// Delete then re-insert same key, should get latest value
void test_delete_and_reinsert_key(void) {
    hash_bucket* bucket; 
	setup_minimal_hash_bucket(&bucket);
    insert_delete_operation_to_resizing_buffer(bucket, 444, "keyx");
    key_value_pair kv = {"keyx", (unsigned char*)"valx", 5};
    insert_node_to_new_operation_buffer(bucket, 444, &kv, false);
	key_value_pair* out = callocate_memory(1, sizeof(key_value_pair));
	int result = get_node_from_resizing_buffer(bucket, 444, "keyx", false, out);
	TEST_ASSERT_EQUAL(SUCCESS, result);
	TEST_ASSERT_EQUAL_STRING("valx", out->value);
	cleanup_hash_bucket_and_buffer(bucket);
	cleanup_key_value_pair(out, false);
}

// Sequence: insert, update, delete, get
void test_sequence_of_operations(void) {
	hash_bucket* bucket; 
	setup_minimal_hash_bucket(&bucket);
	key_value_pair kv = {"seqkey", (unsigned char*)"v1", 3};
	int result = insert_node_to_new_operation_buffer(bucket, 555, &kv, false);
	TEST_ASSERT_EQUAL(SUCCESS_ADDED_TO_PENDING_LIST, result);
	kv.value = (unsigned char*)"v2"; kv.value_size = 3;
	result = insert_update_operation_to_resizing_buffer(bucket, 555, &kv);
	TEST_ASSERT_EQUAL(SUCCESS, result);
	result = insert_delete_operation_to_resizing_buffer(bucket, 555, "seqkey");
	TEST_ASSERT_EQUAL(SUCCESS, result);
	key_value_pair* out = callocate_memory(1, sizeof(key_value_pair));
	result = get_node_from_resizing_buffer(bucket, 555, "seqkey", true, out);
	TEST_ASSERT_EQUAL(ERR_DATA_NODE_NOT_FOUND, result);
	cleanup_hash_bucket_and_buffer(bucket);
	cleanup_key_value_pair(out, false);
}

// Delete buffer when already empty
void test_delete_resizing_buffer_on_empty(void) { 
	hash_bucket* bucket; 
    setup_minimal_hash_bucket(&bucket);
	
    int result = delete_resizing_buffer(bucket->resizing_buffer_ptr);
	bucket->resizing_buffer_ptr = NULL;
	TEST_ASSERT_EQUAL(SUCCESS, result);
	cleanup_hash_bucket_and_buffer(bucket);
}

// --- Tests for commit_resizing_buffer_operations_to_sub_hash_table ---

// Helper: create and initialize a sub_hash_table for the bucket
static void setup_bucket_with_sub_hash_table(hash_bucket **bucket_out, unsigned int bucket_count) {
	hash_bucket* bucket;
	setup_minimal_hash_bucket(&bucket);
	sub_hash_table_configuration config = { .bucket_size = bucket_count, .is_concurrency_enabled = false, .max_linked_list_chain_length = 4 };
	create_new_sub_hash_table(config, true, &bucket->resizing_buffer_ptr->new_sub_hash_table_ptr);
	*bucket_out = bucket;
}

void test_commit_only_upserts(void) {
	hash_bucket* bucket; 
	setup_bucket_with_sub_hash_table(&bucket, 4);
	key_value_pair kv1 = {"k1", (unsigned char*)"v1", 3};
	key_value_pair kv2 = {"k2", (unsigned char*)"v2", 3};
	insert_node_to_new_operation_buffer(bucket, 101, &kv1, false);
	insert_node_to_new_operation_buffer(bucket, 202, &kv2, false);
	int result = commit_resizing_buffer_operations_to_sub_hash_table(bucket);
	TEST_ASSERT_EQUAL(SUCCESS, result);
	key_value_pair* out = callocate_memory(1, sizeof(key_value_pair));
	result = get_key_store_value_from_sub_hash_table(bucket->resizing_buffer_ptr->new_sub_hash_table_ptr, 101, "k1", out);
	TEST_ASSERT_EQUAL(SUCCESS, result);
	TEST_ASSERT_EQUAL_STRING("v1", out->value);
	cleanup_key_value_pair(out, true);
	out = callocate_memory(1, sizeof(key_value_pair));
	result = get_key_store_value_from_sub_hash_table(bucket->resizing_buffer_ptr->new_sub_hash_table_ptr, 202, "k2", out);
	TEST_ASSERT_EQUAL(SUCCESS, result);
	TEST_ASSERT_EQUAL_STRING("v2", out->value);
	cleanup_key_value_pair(out, true);
	cleanup_hash_bucket_and_buffer(bucket);
}

void test_commit_with_updates(void) {
	hash_bucket* bucket; 
	setup_bucket_with_sub_hash_table(&bucket, 4);
	key_value_pair kv = {"k3", (unsigned char*)"v3", 3};
	insert_node_to_new_operation_buffer(bucket, 303, &kv, false);
	kv.value = (unsigned char*)"v3b"; kv.value_size = 4;
	insert_update_operation_to_resizing_buffer(bucket, 303, &kv);
	int result = commit_resizing_buffer_operations_to_sub_hash_table(bucket);
	TEST_ASSERT_EQUAL(SUCCESS, result);
	key_value_pair* out = callocate_memory(1, sizeof(key_value_pair));
	result = get_key_store_value_from_sub_hash_table(bucket->resizing_buffer_ptr->new_sub_hash_table_ptr, 303, "k3", out);
	TEST_ASSERT_EQUAL(SUCCESS, result);
	TEST_ASSERT_EQUAL_STRING("v3b", out->value);
	cleanup_key_value_pair(out, true);
	cleanup_hash_bucket_and_buffer(bucket);
}

void test_commit_with_deletes(void) {
	hash_bucket* bucket; 
	setup_bucket_with_sub_hash_table(&bucket, 4);
	key_value_pair kv = {"k4", (unsigned char*)"v4", 3};
	insert_node_to_new_operation_buffer(bucket, 404, &kv, false);
	insert_delete_operation_to_resizing_buffer(bucket, 404, "k4");
	int result = commit_resizing_buffer_operations_to_sub_hash_table(bucket);
	TEST_ASSERT_EQUAL(SUCCESS, result);
	key_value_pair* out = callocate_memory(1, sizeof(key_value_pair));
	result = get_key_store_value_from_sub_hash_table(bucket->resizing_buffer_ptr->new_sub_hash_table_ptr, 404, "k4", out);
	TEST_ASSERT_LESS_THAN(0, result); // Not found
	cleanup_key_value_pair(out, false);
	cleanup_hash_bucket_and_buffer(bucket);
}

void test_commit_mixed_operations(void) {
	hash_bucket* bucket; 
	setup_bucket_with_sub_hash_table(&bucket, 4);
	key_value_pair kv1 = {"k5", (unsigned char*)"v5", 3};
	key_value_pair kv2 = {"k6", (unsigned char*)"v6", 3};
	insert_node_to_new_operation_buffer(bucket, 505, &kv1, false);
	insert_node_to_new_operation_buffer(bucket, 606, &kv2, false);
	insert_delete_operation_to_resizing_buffer(bucket, 505, "k5");
	key_value_pair kv2b = {"k6", (unsigned char*)"v6b", 4};
	insert_update_operation_to_resizing_buffer(bucket, 606, &kv2b);
	int result = commit_resizing_buffer_operations_to_sub_hash_table(bucket);
	TEST_ASSERT_EQUAL(SUCCESS, result);
	key_value_pair* out = callocate_memory(1, sizeof(key_value_pair));
	result = get_key_store_value_from_sub_hash_table(bucket->resizing_buffer_ptr->new_sub_hash_table_ptr, 505, "k5", out);
	TEST_ASSERT_LESS_THAN(0, result); // k5 deleted
	cleanup_key_value_pair(out, true);
	out = callocate_memory(1, sizeof(key_value_pair));
	result = get_key_store_value_from_sub_hash_table(bucket->resizing_buffer_ptr->new_sub_hash_table_ptr, 606, "k6", out);
	TEST_ASSERT_EQUAL(SUCCESS, result);
	TEST_ASSERT_EQUAL_STRING("v6b", out->value);
	cleanup_key_value_pair(out, true);
	cleanup_hash_bucket_and_buffer(bucket);
}

void test_commit_empty_buffers(void) {
	hash_bucket* bucket; 
	setup_bucket_with_sub_hash_table(&bucket, 4);
	int result = commit_resizing_buffer_operations_to_sub_hash_table(bucket);
	TEST_ASSERT_EQUAL(SUCCESS, result);
	cleanup_hash_bucket_and_buffer(bucket);
}

void test_commit_null_argument(void) {
	TEST_ASSERT_EQUAL(ERR_INVALID_ARGUMENT, commit_resizing_buffer_operations_to_sub_hash_table(NULL));
}

void test_commit_after_buffer_deleted(void) {
	hash_bucket* bucket; 
	setup_bucket_with_sub_hash_table(&bucket, 4);
	key_value_pair kv = {"k7", (unsigned char*)"v7", 3};
	insert_node_to_new_operation_buffer(bucket, 707, &kv, false);
	delete_resizing_buffer(bucket->resizing_buffer_ptr);
	bucket->resizing_buffer_ptr = NULL;
	int result = commit_resizing_buffer_operations_to_sub_hash_table(bucket);
	TEST_ASSERT_EQUAL(ERR_INVALID_ARGUMENT, result);
	cleanup_hash_bucket_and_buffer(bucket);
}

void test_commit_duplicate_keys(void) {
	hash_bucket* bucket; 
	setup_bucket_with_sub_hash_table(&bucket, 4);
	key_value_pair kv = {"dup", (unsigned char*)"v1", 3};
	insert_node_to_new_operation_buffer(bucket, 808, &kv, false);
	kv.value = (unsigned char*)"v2"; kv.value_size = 3;
	insert_node_to_new_operation_buffer(bucket, 808, &kv, false);
	insert_delete_operation_to_resizing_buffer(bucket, 808, "dup");
	key_value_pair kv3 = {"dup", (unsigned char*)"v3", 3};
	insert_node_to_new_operation_buffer(bucket, 808, &kv3, false);
	int result = commit_resizing_buffer_operations_to_sub_hash_table(bucket);
	TEST_ASSERT_EQUAL(SUCCESS, result);
	key_value_pair* out = callocate_memory(1, sizeof(key_value_pair));
	result = get_key_store_value_from_sub_hash_table(bucket->resizing_buffer_ptr->new_sub_hash_table_ptr, 808, "dup", out);
	TEST_ASSERT_EQUAL(ERR_DATA_NODE_NOT_FOUND, result);
	cleanup_key_value_pair(out, false);
	cleanup_hash_bucket_and_buffer(bucket);
}


int test_buffer_operation_main(void) {
	UNITY_BEGIN();
	printf("Running Buffer Operation Unit Tests...\n");
	RUN_TEST(test_upsert_node_to_resizing_buffer_null_args);
	RUN_TEST(test_upsert_and_get_node_from_resizing_buffer);
	RUN_TEST(test_insert_delete_operation_to_resizing_buffer_and_get);
	RUN_TEST(test_insert_update_operation_to_resizing_buffer_and_get);
	RUN_TEST(test_get_node_from_resizing_buffer_not_found);
	RUN_TEST(test_delete_resizing_buffer_null);
	RUN_TEST(test_delete_resizing_buffer_valid);

	// Additional coverage tests
	RUN_TEST(test_upsert_duplicate_key_and_get_latest);
	RUN_TEST(test_upsert_with_value_size_zero);
	RUN_TEST(test_insert_update_with_null_value);
	RUN_TEST(test_delete_and_reinsert_key);
	RUN_TEST(test_sequence_of_operations);
	RUN_TEST(test_delete_resizing_buffer_on_empty);

	// Commit buffer operation tests
	RUN_TEST(test_commit_only_upserts);
	RUN_TEST(test_commit_with_updates);
	RUN_TEST(test_commit_with_deletes);
	RUN_TEST(test_commit_mixed_operations);
	RUN_TEST(test_commit_empty_buffers);
	RUN_TEST(test_commit_null_argument);
	RUN_TEST(test_commit_after_buffer_deleted);
	RUN_TEST(test_commit_duplicate_keys);
	printf("Completed Buffer Operation Unit Tests.\n");
	return UNITY_END();
}

