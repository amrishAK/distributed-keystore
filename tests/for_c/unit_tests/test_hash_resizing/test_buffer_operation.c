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
	if (bucket) {
		if (bucket->resizing_buffer_ptr) {
			if (bucket->resizing_buffer_ptr->new_sub_hash_table_ptr) {
				cleanup_sub_hash_table(bucket->resizing_buffer_ptr->new_sub_hash_table_ptr);
				free_memory(bucket->resizing_buffer_ptr->new_sub_hash_table_ptr, false);
				bucket->resizing_buffer_ptr->new_sub_hash_table_ptr = NULL;
			}
			delete_resizing_buffer(bucket->resizing_buffer_ptr);
			free_memory(bucket->resizing_buffer_ptr, false);
			bucket->resizing_buffer_ptr = NULL;
		}
		cleanup_hash_bucket(bucket);
		free_memory(bucket, false);
	}
}

static composite_key_hash composite_key_helper(uint64_t bucket_hash, uint64_t sub_bucket_hash) {
    composite_key_hash out = {0};
    out.bucket_hash = bucket_hash;
    out.sub_bucket_hash = sub_bucket_hash;
    return out;
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
	// Arrange
	composite_key_hash key_hash = composite_key_helper(1,1);

	// Act & Assert
	TEST_ASSERT_EQUAL(ERR_INVALID_ARGUMENT, insert_node_to_new_operation_buffer(NULL, key_hash, NULL, false));

	hash_bucket* bucket;
	setup_minimal_hash_bucket(&bucket);
	TEST_ASSERT_EQUAL(ERR_INVALID_ARGUMENT, insert_node_to_new_operation_buffer(bucket, key_hash, NULL, false));

	// Cleanup
	cleanup_hash_bucket_and_buffer(bucket);
}

void test_upsert_and_get_node_from_resizing_buffer(void) {
	// Arrange
	hash_bucket* bucket;
	composite_key_hash key_hash = composite_key_helper(123, 456);
	setup_minimal_hash_bucket(&bucket);
	key_value_pair kv = {"key1", (unsigned char*)"val1", 5};

	// Act
	int result = insert_node_to_new_operation_buffer(bucket, key_hash, &kv, false);

	// Assert
	TEST_ASSERT_EQUAL(SUCCESS_ADDED_TO_PENDING_LIST, result);
	key_value_pair* out = callocate_memory(1, sizeof(key_value_pair));
	result = get_node_from_resizing_buffer(bucket, key_hash, "key1", false, out);
	TEST_ASSERT_EQUAL(SUCCESS, result);
	TEST_ASSERT_EQUAL_STRING("key1", out->key);
	TEST_ASSERT_EQUAL_STRING("val1", out->value);

	// Cleanup
	cleanup_hash_bucket_and_buffer(bucket);
	cleanup_key_value_pair(out, true);
}

void test_insert_delete_operation_to_resizing_buffer_and_get(void) {
	// Arrange
	hash_bucket* bucket;
	setup_minimal_hash_bucket(&bucket);
	composite_key_hash key_hash = composite_key_helper(0, 456);

	// Act
	int result = insert_delete_operation_to_resizing_buffer(bucket, key_hash, "key2");

	// Assert
	TEST_ASSERT_EQUAL(SUCCESS, result);
	key_value_pair* out = callocate_memory(1, sizeof(key_value_pair));
	result = get_node_from_resizing_buffer(bucket, key_hash, "key2", true, out);
	TEST_ASSERT_EQUAL(ERR_DATA_NODE_NOT_FOUND, result);

	// Cleanup
	cleanup_hash_bucket_and_buffer(bucket);
	cleanup_key_value_pair(out, true);
}

void test_insert_update_operation_to_resizing_buffer_and_get(void) {
	// Arrange
	hash_bucket* bucket;
	composite_key_hash key_hash = composite_key_helper(0,789);
	setup_minimal_hash_bucket(&bucket);
	key_value_pair kv = {"key3", (unsigned char*)"val3", 5};

	// Act
	int result = insert_update_operation_to_resizing_buffer(bucket, key_hash, &kv);

	// Assert
	TEST_ASSERT_EQUAL(SUCCESS, result);
	key_value_pair* out = callocate_memory(1, sizeof(key_value_pair));
	result = get_node_from_resizing_buffer(bucket, key_hash, "key3", true, out);
	TEST_ASSERT_EQUAL(SUCCESS, result);
	TEST_ASSERT_EQUAL_STRING("key3", out->key);
	TEST_ASSERT_EQUAL_STRING("val3", out->value);

	// Cleanup
	cleanup_hash_bucket_and_buffer(bucket);
	cleanup_key_value_pair(out, true);
}

void test_get_node_from_resizing_buffer_not_found(void) {
	// Arrange
	hash_bucket* bucket;
	setup_minimal_hash_bucket(&bucket);
	composite_key_hash key_hash = composite_key_helper(0, 999);

	// Act
	key_value_pair* out = callocate_memory(1, sizeof(key_value_pair));
	int result = get_node_from_resizing_buffer(bucket, key_hash, "notfound", false, out);

	// Assert
	TEST_ASSERT_EQUAL(ERR_DATA_NODE_NOT_FOUND, result);

	// Cleanup
	cleanup_hash_bucket_and_buffer(bucket);
	cleanup_key_value_pair(out, true);
}

void test_delete_resizing_buffer_null(void) {
	// Act & Assert
	TEST_ASSERT_EQUAL(ERR_INVALID_ARGUMENT, delete_resizing_buffer(NULL));
}

void test_delete_resizing_buffer_valid(void) {
	// Arrange
	hash_bucket* bucket;
	setup_minimal_hash_bucket(&bucket);
	composite_key_hash key_hash = composite_key_helper(0, 321);
	key_value_pair kv = {"key4", (unsigned char*)"val4", 5};

	// Act
	insert_node_to_new_operation_buffer(bucket, key_hash, &kv, false);
	int result = delete_resizing_buffer(bucket->resizing_buffer_ptr);
    free_memory(bucket->resizing_buffer_ptr, false);
	bucket->resizing_buffer_ptr = NULL;

	// Assert
	TEST_ASSERT_EQUAL(SUCCESS, result);

	// Cleanup
	cleanup_hash_bucket_and_buffer(bucket);
}

// Insert duplicate key and ensure latest is returned
void test_upsert_duplicate_key_and_get_latest(void) {
	// Arrange
	hash_bucket* bucket;
	setup_minimal_hash_bucket(&bucket);
	key_value_pair kv1 = {"dupkey", (unsigned char*)"val1", 5};
	key_value_pair kv2 = {"dupkey", (unsigned char*)"val2", 5};
	composite_key_hash key_hash = composite_key_helper(0, 111);

	// Act
	insert_node_to_new_operation_buffer(bucket, key_hash, &kv1, false);
	insert_node_to_new_operation_buffer(bucket, key_hash, &kv2, false);

	// Assert
	key_value_pair* out = callocate_memory(1, sizeof(key_value_pair));
	int result = get_node_from_resizing_buffer(bucket, key_hash, "dupkey", false, out);
	TEST_ASSERT_EQUAL(SUCCESS, result);
	TEST_ASSERT_EQUAL_STRING("val2", out->value);

	// Cleanup
	cleanup_hash_bucket_and_buffer(bucket);
	cleanup_key_value_pair(out, true);
}

// Upsert with value_size = 0 should mark as deleted
void test_upsert_with_value_size_zero(void) {
	// Arrange
	hash_bucket* bucket;
	setup_minimal_hash_bucket(&bucket);
	key_value_pair kv = {"keydel", (unsigned char*)"", 0};
	composite_key_hash key_hash = composite_key_helper(0, 222);

	// Act
	insert_node_to_new_operation_buffer(bucket, key_hash, &kv, false);

	// Assert
	key_value_pair* out = callocate_memory(1, sizeof(key_value_pair));
	int result = get_node_from_resizing_buffer(bucket, key_hash, "keydel", false, out);
	TEST_ASSERT_EQUAL(SUCCESS, result);
	TEST_ASSERT_EQUAL(0, out->value_size);

	// Cleanup
	cleanup_hash_bucket_and_buffer(bucket);
	cleanup_key_value_pair(out, true);
}

// Insert update with NULL value or size 0 should error
void test_insert_update_with_null_value(void) {
	// Arrange
	hash_bucket* bucket;
	setup_minimal_hash_bucket(&bucket);
	key_value_pair kv = {"keyupd", NULL, 0};
	composite_key_hash key_hash = composite_key_helper(0, 333);

	// Act
	int result = insert_update_operation_to_resizing_buffer(bucket, key_hash, &kv);

	// Assert
	TEST_ASSERT_EQUAL(ERR_INVALID_ARGUMENT, result);

	// Cleanup
	cleanup_hash_bucket_and_buffer(bucket);
}

// Delete then re-insert same key, should get latest value
void test_delete_and_reinsert_key(void) {
	// Arrange
	hash_bucket* bucket;
	setup_minimal_hash_bucket(&bucket);
	composite_key_hash key_hash = composite_key_helper(0, 444);

	// Act
	insert_delete_operation_to_resizing_buffer(bucket, key_hash, "keyx");
	key_value_pair kv = {"keyx", (unsigned char*)"valx", 5};
	insert_node_to_new_operation_buffer(bucket, key_hash, &kv, false);

	// Assert
	key_value_pair* out = callocate_memory(1, sizeof(key_value_pair));
	int result = get_node_from_resizing_buffer(bucket, key_hash, "keyx", false, out);
	TEST_ASSERT_EQUAL(SUCCESS, result);
	TEST_ASSERT_EQUAL_STRING("valx", out->value);

	// Cleanup
	cleanup_hash_bucket_and_buffer(bucket);
	cleanup_key_value_pair(out, true);
}

// Sequence: insert, update, delete, get
void test_sequence_of_operations(void) {
	// Arrange
	hash_bucket* bucket;
	setup_minimal_hash_bucket(&bucket);
	key_value_pair kv = {"seqkey", (unsigned char*)"v1", 3};
	composite_key_hash key_hash = composite_key_helper(0, 555);

	// Act
	int result = insert_node_to_new_operation_buffer(bucket, key_hash, &kv, false);
	TEST_ASSERT_EQUAL(SUCCESS_ADDED_TO_PENDING_LIST, result);
	kv.value = (unsigned char*)"v2"; kv.value_size = 3;
	result = insert_update_operation_to_resizing_buffer(bucket, key_hash, &kv);
	TEST_ASSERT_EQUAL(SUCCESS, result);
	result = insert_delete_operation_to_resizing_buffer(bucket, key_hash, "seqkey");
	TEST_ASSERT_EQUAL(SUCCESS, result);

	// Assert
	key_value_pair* out = callocate_memory(1, sizeof(key_value_pair));
	result = get_node_from_resizing_buffer(bucket, key_hash, "seqkey", true, out);
	TEST_ASSERT_EQUAL(ERR_DATA_NODE_NOT_FOUND, result);

	// Cleanup
	cleanup_hash_bucket_and_buffer(bucket);
	cleanup_key_value_pair(out, true);
}

// Delete buffer when already empty
void test_delete_resizing_buffer_on_empty(void) {
	// Arrange
	hash_bucket* bucket;
	setup_minimal_hash_bucket(&bucket);

	// Act
	int result = delete_resizing_buffer(bucket->resizing_buffer_ptr);
    free_memory(bucket->resizing_buffer_ptr, false);
	bucket->resizing_buffer_ptr = NULL;

	// Assert
	TEST_ASSERT_EQUAL(SUCCESS, result);

	// Cleanup
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
	// Arrange
	hash_bucket* bucket;
	setup_bucket_with_sub_hash_table(&bucket, 4);
	key_value_pair kv1 = {"k1", (unsigned char*)"v1", 3};
	key_value_pair kv2 = {"k2", (unsigned char*)"v2", 3};
	composite_key_hash key_hash1 = composite_key_helper(1, 101);
	composite_key_hash key_hash2 = composite_key_helper(1, 202);

	// Act
	int result = insert_node_to_new_operation_buffer(bucket, key_hash1, &kv1, false);
    TEST_ASSERT_EQUAL(SUCCESS_ADDED_TO_PENDING_LIST, result);
	result = insert_node_to_new_operation_buffer(bucket, key_hash2, &kv2, false);
    TEST_ASSERT_EQUAL(SUCCESS_ADDED_TO_PENDING_LIST, result);
	result = commit_resizing_buffer_operations_to_sub_hash_table(bucket);

	// Assert
	TEST_ASSERT_EQUAL(SUCCESS, result);

	key_value_pair* out = callocate_memory(1, sizeof(key_value_pair));
	result = get_key_store_value_from_sub_hash_table(bucket->resizing_buffer_ptr->new_sub_hash_table_ptr, key_hash1, "k1", out);
	TEST_ASSERT_EQUAL(SUCCESS, result);
	TEST_ASSERT_EQUAL_STRING("v1", out->value);
	cleanup_key_value_pair(out, true);

	key_value_pair *out2 = callocate_memory(1, sizeof(key_value_pair));
	result = get_key_store_value_from_sub_hash_table(bucket->resizing_buffer_ptr->new_sub_hash_table_ptr, key_hash2, "k2", out2);
	TEST_ASSERT_EQUAL(SUCCESS, result);
	TEST_ASSERT_EQUAL_STRING("v2", out2->value);
	cleanup_key_value_pair(out2, true);

	// Cleanup
	cleanup_hash_bucket_and_buffer(bucket);
}

void test_commit_with_updates(void) {
	hash_bucket* bucket; 
	setup_bucket_with_sub_hash_table(&bucket, 4);
	key_value_pair kv = {"k3", (unsigned char*)"v3", 3};
    composite_key_hash key_hash = composite_key_helper(0, 303);
	insert_node_to_new_operation_buffer(bucket, key_hash, &kv, false);
	kv.value = (unsigned char*)"v3b"; kv.value_size = 4;
	insert_update_operation_to_resizing_buffer(bucket, key_hash, &kv);
	int result = commit_resizing_buffer_operations_to_sub_hash_table(bucket);
	TEST_ASSERT_EQUAL(SUCCESS, result);
	key_value_pair* out = callocate_memory(1, sizeof(key_value_pair));
	result = get_key_store_value_from_sub_hash_table(bucket->resizing_buffer_ptr->new_sub_hash_table_ptr, key_hash, "k3", out);
	TEST_ASSERT_EQUAL(SUCCESS, result);
	TEST_ASSERT_EQUAL_STRING("v3b", out->value);
	cleanup_key_value_pair(out, true);
	cleanup_hash_bucket_and_buffer(bucket);
}

void test_commit_with_deletes(void) {
	hash_bucket* bucket; 
	setup_bucket_with_sub_hash_table(&bucket, 4);
	key_value_pair kv = {"k4", (unsigned char*)"v4", 3};
    composite_key_hash key_hash = composite_key_helper(0, 404);
	insert_node_to_new_operation_buffer(bucket, key_hash, &kv, false);
	insert_delete_operation_to_resizing_buffer(bucket, key_hash, "k4");
	int result = commit_resizing_buffer_operations_to_sub_hash_table(bucket);
	TEST_ASSERT_EQUAL(SUCCESS, result);
	key_value_pair* out = callocate_memory(1, sizeof(key_value_pair));
	result = get_key_store_value_from_sub_hash_table(bucket->resizing_buffer_ptr->new_sub_hash_table_ptr, key_hash, "k4", out);
	TEST_ASSERT_LESS_THAN(0, result); // Not found
	cleanup_key_value_pair(out, true);
	cleanup_hash_bucket_and_buffer(bucket);
}

void test_commit_mixed_operations(void) {
	hash_bucket* bucket; 
	setup_bucket_with_sub_hash_table(&bucket, 4);
	key_value_pair kv1 = {"k5", (unsigned char*)"v5", 3};
	key_value_pair kv2 = {"k6", (unsigned char*)"v6", 3};
    composite_key_hash key_hash1 = composite_key_helper(0, 505);
    composite_key_hash key_hash2 = composite_key_helper(0, 606);
	insert_node_to_new_operation_buffer(bucket, key_hash1, &kv1, false);
	insert_node_to_new_operation_buffer(bucket, key_hash2, &kv2, false);
	insert_delete_operation_to_resizing_buffer(bucket, key_hash1, "k5");
	key_value_pair kv2b = {"k6", (unsigned char*)"v6b", 4};
	insert_update_operation_to_resizing_buffer(bucket, key_hash2, &kv2b);
	int result = commit_resizing_buffer_operations_to_sub_hash_table(bucket);
	TEST_ASSERT_EQUAL(SUCCESS, result);
	key_value_pair* out = callocate_memory(1, sizeof(key_value_pair));
	result = get_key_store_value_from_sub_hash_table(bucket->resizing_buffer_ptr->new_sub_hash_table_ptr, key_hash1, "k5", out);
	TEST_ASSERT_LESS_THAN(0, result); // k5 deleted
	cleanup_key_value_pair(out, true);
	out = callocate_memory(1, sizeof(key_value_pair));
	result = get_key_store_value_from_sub_hash_table(bucket->resizing_buffer_ptr->new_sub_hash_table_ptr, key_hash2, "k6", out);
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
    composite_key_hash key_hash = composite_key_helper(0, 707);
	insert_node_to_new_operation_buffer(bucket, key_hash, &kv, false);
	delete_resizing_buffer(bucket->resizing_buffer_ptr);
    free_memory(bucket->resizing_buffer_ptr, false);
	bucket->resizing_buffer_ptr = NULL;
	int result = commit_resizing_buffer_operations_to_sub_hash_table(bucket);
	TEST_ASSERT_EQUAL(ERR_INVALID_ARGUMENT, result);
	cleanup_hash_bucket_and_buffer(bucket);
}

void test_commit_duplicate_keys(void) {
	hash_bucket* bucket; 
	setup_bucket_with_sub_hash_table(&bucket, 4);
	key_value_pair kv = {"dup", (unsigned char*)"v1", 3};
    composite_key_hash key_hash = composite_key_helper(0, 808);
	insert_node_to_new_operation_buffer(bucket, key_hash, &kv, false);
	kv.value = (unsigned char*)"v2"; kv.value_size = 3;
	insert_node_to_new_operation_buffer(bucket, key_hash, &kv, false);
	insert_delete_operation_to_resizing_buffer(bucket, key_hash, "dup");
	key_value_pair kv3 = {"dup", (unsigned char*)"v3", 3};
	insert_node_to_new_operation_buffer(bucket, key_hash, &kv3, false);
	int result = commit_resizing_buffer_operations_to_sub_hash_table(bucket);
	TEST_ASSERT_EQUAL(SUCCESS, result);
	key_value_pair* out = callocate_memory(1, sizeof(key_value_pair));
	result = get_key_store_value_from_sub_hash_table(bucket->resizing_buffer_ptr->new_sub_hash_table_ptr, key_hash, "dup", out);
	TEST_ASSERT_EQUAL(ERR_DATA_NODE_NOT_FOUND, result);
	cleanup_key_value_pair(out, true);
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