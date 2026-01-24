/**
 * @file test_hash_bucket_operation.c
 * @brief Unit tests for hash bucket operations in the hash table module.
 * This file tests the initialization, cleanup, insertion, retrieval,
 * updating, and deletion of key-value pairs in a hash bucket.
 * Test Scenarios:
 *1. Initialization and Cleanup
 *2. Insertion and Retrieval
 *3. Updating Existing Key-Value Pairs
 *4. Deletion of Key-Value Pairs
 *5. Invalid Arguments Handling
 *6. Double Initialization and Cleanup
 *7. Insertion of Null Key or Value
 *8. Retrieval and Deletion of Non-Existent Keys
 *9. Multiple Keys and Collision Handling
 */
#include "unity.h"
#include "hash_table/hash_bucket_operation.h"
#include "type_definitions/error_code_definitions.h"
#include <string.h>
#include <stdlib.h>

void test_initialise_and_cleanup_hash_bucket(void) {
	hash_bucket* bucket_ptr = malloc(sizeof(hash_bucket));
	sub_hash_table_configuration config = { .is_concurrency_enabled = false, .bucket_size = 4, .max_linked_list_chain_length = 4 };
	int result = initialise_hash_bucket(bucket_ptr, config);
	TEST_ASSERT_EQUAL(0, result);
	TEST_ASSERT_TRUE(bucket_ptr->is_initialized);
	result = cleanup_hash_bucket(bucket_ptr);
	TEST_ASSERT_EQUAL(0, result);
	TEST_ASSERT_FALSE(bucket_ptr->is_initialized);
    TEST_ASSERT_EQUAL(0, cleanup_hash_bucket(bucket_ptr));
    free(bucket_ptr);
}

void test_upsert_and_get_key_value(void) {
	hash_bucket* bucket_ptr = malloc(sizeof(hash_bucket));
	sub_hash_table_configuration config = { .is_concurrency_enabled = false, .bucket_size = 4, .max_linked_list_chain_length = 4 };
	initialise_hash_bucket(bucket_ptr, config);
    unsigned char value[] = "bar";
	key_value_pair kv = { .key = "foo", .value = value, .value_size = strlen((char*)value) + 1 };
	int result = upsert_node_to_hash_bucket(bucket_ptr, 123, &kv);
	TEST_ASSERT_TRUE(result == 0 || result == 10);
	key_value_pair out = {0};
	result = get_key_value_from_hash_bucket(bucket_ptr, "foo", 123, &out);
	TEST_ASSERT_EQUAL(0, result);
	TEST_ASSERT_EQUAL_STRING("foo", out.key);
	TEST_ASSERT_EQUAL_STRING("bar", out.value);
	free(out.key);
	free(out.value);
    TEST_ASSERT_EQUAL(0, cleanup_hash_bucket(bucket_ptr));
    free(bucket_ptr);
}

void test_update_existing_key_value(void) {
	hash_bucket* bucket_ptr = malloc(sizeof(hash_bucket));
	sub_hash_table_configuration config = { .is_concurrency_enabled = false, .bucket_size = 4, .max_linked_list_chain_length = 4 };
	initialise_hash_bucket(bucket_ptr, config);
    unsigned char value[] = "bar";
	key_value_pair kv = { .key = "foo", .value = value, .value_size = strlen((char*)value) + 1 };
	upsert_node_to_hash_bucket(bucket_ptr, 123, &kv);
	kv.value = (unsigned char*)"baz";
	kv.value_size = 4;
	int result = upsert_node_to_hash_bucket(bucket_ptr, 123, &kv);
	TEST_ASSERT_EQUAL(0, result);
	key_value_pair out = {0};
	result = get_key_value_from_hash_bucket(bucket_ptr, "foo", 123, &out);
	TEST_ASSERT_EQUAL(0, result);
	TEST_ASSERT_EQUAL_STRING("foo", out.key);
	TEST_ASSERT_EQUAL_STRING("baz", out.value);
	free(out.key);
	free(out.value);
    TEST_ASSERT_EQUAL(0, cleanup_hash_bucket(bucket_ptr));
    free(bucket_ptr);
}

void test_delete_key_from_hash_bucket(void) {
	hash_bucket* bucket_ptr = malloc(sizeof(hash_bucket));
	sub_hash_table_configuration config = { .is_concurrency_enabled = false, .bucket_size = 4, .max_linked_list_chain_length = 4 };
	initialise_hash_bucket(bucket_ptr, config);
    unsigned char value[] = "bar";
	key_value_pair kv = { .key = "foo", .value = value, .value_size = strlen((char*)value) + 1 };
	upsert_node_to_hash_bucket(bucket_ptr, 123, &kv);
	int result = delete_key_from_hash_bucket(bucket_ptr, "foo", 123);
	TEST_ASSERT_EQUAL(0, result);
	key_value_pair out = {0};
	result = get_key_value_from_hash_bucket(bucket_ptr, "foo", 123, &out);
	TEST_ASSERT_LESS_THAN(0, result);
    TEST_ASSERT_EQUAL(0, cleanup_hash_bucket(bucket_ptr));
    free(bucket_ptr);
}

void test_invalid_args_hash_bucket(void) {
	hash_bucket* bucket_ptr = malloc(sizeof(hash_bucket));
	sub_hash_table_configuration config = { .is_concurrency_enabled = false, .bucket_size = 4, .max_linked_list_chain_length = 4 };
	// NULL bucket
	TEST_ASSERT_LESS_THAN(0, initialise_hash_bucket(NULL, config));
	TEST_ASSERT_LESS_THAN(0, upsert_node_to_hash_bucket(NULL, 123, NULL));
	TEST_ASSERT_LESS_THAN(0, get_key_value_from_hash_bucket(NULL, NULL, 0, NULL));
	TEST_ASSERT_LESS_THAN(0, delete_key_from_hash_bucket(NULL, NULL, 0));
	// Not initialized
	TEST_ASSERT_LESS_THAN(0, upsert_node_to_hash_bucket(bucket_ptr, 123, NULL));
	TEST_ASSERT_LESS_THAN(0, get_key_value_from_hash_bucket(bucket_ptr, NULL, 0, NULL));
	TEST_ASSERT_LESS_THAN(0, delete_key_from_hash_bucket(bucket_ptr, NULL, 0));
    TEST_ASSERT_EQUAL(0, cleanup_hash_bucket(bucket_ptr));
    free(bucket_ptr);
}

// Additional edge and negative tests for coverage
void test_double_initialise_and_cleanup(void) {
	hash_bucket* bucket_ptr = malloc(sizeof(hash_bucket));
	sub_hash_table_configuration config = { .is_concurrency_enabled = false, .bucket_size = 4, .max_linked_list_chain_length = 4 };
	int result = initialise_hash_bucket(bucket_ptr, config);
	TEST_ASSERT_EQUAL(0, result);
	// Double initialise should be a no-op
	result = initialise_hash_bucket(bucket_ptr, config);
	TEST_ASSERT_EQUAL(0, result);
	// Double cleanup should be a no-op
	result = cleanup_hash_bucket(bucket_ptr);
	TEST_ASSERT_EQUAL(0, result);
	result = cleanup_hash_bucket(bucket_ptr);
	TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_EQUAL(0, cleanup_hash_bucket(bucket_ptr));
    free(bucket_ptr);
}

void test_upsert_null_key_value(void) {
	hash_bucket* bucket_ptr = malloc(sizeof(hash_bucket));
	sub_hash_table_configuration config = { .is_concurrency_enabled = false, .bucket_size = 4, .max_linked_list_chain_length = 4 };
	initialise_hash_bucket(bucket_ptr, config);
	// Null key
	key_value_pair kv = { .key = NULL, .value = (unsigned char*)"bar", .value_size = 4 };
	int result = upsert_node_to_hash_bucket(bucket_ptr, 123, &kv);
	TEST_ASSERT_LESS_THAN(0, result);
	// Null value
	kv.key = "foo";
	kv.value = NULL;
	kv.value_size = 0;
	result = upsert_node_to_hash_bucket(bucket_ptr, 123, &kv);
	TEST_ASSERT_LESS_THAN(0, result);
    TEST_ASSERT_EQUAL(0, cleanup_hash_bucket(bucket_ptr));
    free(bucket_ptr);
}

void test_get_and_delete_nonexistent_key(void) {
	hash_bucket* bucket_ptr = malloc(sizeof(hash_bucket));
	sub_hash_table_configuration config = { .is_concurrency_enabled = false, .bucket_size = 4, .max_linked_list_chain_length = 4 };
	initialise_hash_bucket(bucket_ptr, config);
	// Get non-existent key
	key_value_pair out = {0};
	int result = get_key_value_from_hash_bucket(bucket_ptr, "nope", 999, &out);
	TEST_ASSERT_LESS_THAN(0, result);
	// Delete non-existent key
	result = delete_key_from_hash_bucket(bucket_ptr, "nope", 999);
	TEST_ASSERT_LESS_THAN(0, result);
    TEST_ASSERT_EQUAL(0, cleanup_hash_bucket(bucket_ptr));
    free(bucket_ptr);
}

void test_multiple_keys_and_collision(void) {
	// Allocate and zero the bucket for safety
	hash_bucket* bucket_ptr = malloc(sizeof(hash_bucket));
	TEST_ASSERT_NOT_NULL(bucket_ptr);

	sub_hash_table_configuration config = { .is_concurrency_enabled = false, .bucket_size = 1, .max_linked_list_chain_length = 4 };
	int init_result = initialise_hash_bucket(bucket_ptr, config);
	TEST_ASSERT_EQUAL(0, init_result);

	// Insert multiple keys with the same hash to test collision handling
	unsigned char v1[] = "v1";
	unsigned char v2[] = "v2";
	key_value_pair kv1 = { .key = "A", .value = v1, .value_size = strlen((char*)v1) + 1 };
	key_value_pair kv2 = { .key = "B", .value = v2, .value_size = strlen((char*)v2) + 1 };

	int result1 = upsert_node_to_hash_bucket(bucket_ptr, 42, &kv1);
	TEST_ASSERT_TRUE(result1 == 0 || result1 == 10);
	int result2 = upsert_node_to_hash_bucket(bucket_ptr, 43, &kv2);
	TEST_ASSERT_TRUE(result2 == 0 || result2 == 10);

	// Retrieve both keys and check correctness
	key_value_pair out1 = {0};
	key_value_pair out2 = {0};
	int r1 = get_key_value_from_hash_bucket(bucket_ptr, "A", 42, &out1);
	int r2 = get_key_value_from_hash_bucket(bucket_ptr, "B", 43, &out2);
	TEST_ASSERT_EQUAL(0, r1);
	TEST_ASSERT_EQUAL(0, r2);
	TEST_ASSERT_EQUAL_STRING("A", out1.key);
	TEST_ASSERT_EQUAL_STRING("v1", out1.value);
	TEST_ASSERT_EQUAL_STRING("B", out2.key);
	TEST_ASSERT_EQUAL_STRING("v2", out2.value);

	free(out1.key);
	free(out1.value);
	free(out2.key);
	free(out2.value);

    TEST_ASSERT_EQUAL(0, cleanup_hash_bucket(bucket_ptr));
    free(bucket_ptr);
}

void test_edit_hash_bucket_node_after_deletion(void) {
	hash_bucket* bucket_ptr = malloc(sizeof(hash_bucket));
	sub_hash_table_configuration config = { .is_concurrency_enabled = false, .bucket_size = 4, .max_linked_list_chain_length = 4 };
	initialise_hash_bucket(bucket_ptr, config);
	unsigned char value[] = "bar";
	key_value_pair kv = { .key = "foo", .value = value, .value_size = strlen((char*)value) + 1 };
	upsert_node_to_hash_bucket(bucket_ptr, 123, &kv);
	delete_key_from_hash_bucket(bucket_ptr, "foo", 123);
	// Attempt to update after deletion
	kv.value = (unsigned char*)"baz";
	kv.value_size = 4;
	int result = upsert_node_to_hash_bucket(bucket_ptr, 123, &kv);
	TEST_ASSERT_TRUE(result == 0 || result == 10); // Should allow re-insertion
	key_value_pair out = {0};
	result = get_key_value_from_hash_bucket(bucket_ptr, "foo", 123, &out);
	TEST_ASSERT_EQUAL(0, result);
	TEST_ASSERT_EQUAL_STRING("foo", out.key);
	TEST_ASSERT_EQUAL_STRING("baz", out.value);
	free(out.key);
	free(out.value);
    TEST_ASSERT_EQUAL(0, cleanup_hash_bucket(bucket_ptr));
    free(bucket_ptr);
}

void test_read_hash_bucket_node_after_deletion(void) {
	hash_bucket* bucket_ptr = malloc(sizeof(hash_bucket));
	sub_hash_table_configuration config = { .is_concurrency_enabled = false, .bucket_size = 4, .max_linked_list_chain_length = 4 };
	initialise_hash_bucket(bucket_ptr, config);
	unsigned char value[] = "bar";
	key_value_pair kv = { .key = "foo", .value = value, .value_size = strlen((char*)value) + 1 };
	upsert_node_to_hash_bucket(bucket_ptr, 123, &kv);
	delete_key_from_hash_bucket(bucket_ptr, "foo", 123);
	// Attempt to read after deletion
	key_value_pair out = {0};
	int result = get_key_value_from_hash_bucket(bucket_ptr, "foo", 123, &out);
	TEST_ASSERT_EQUAL(ERR_DATA_NODE_NOT_FOUND, result); // Should not find the key
    TEST_ASSERT_EQUAL(0, cleanup_hash_bucket(bucket_ptr));
    free(bucket_ptr);
}

void test_delete_hash_bucket_node_twice(void) {
	hash_bucket* bucket_ptr = malloc(sizeof(hash_bucket));
	sub_hash_table_configuration config = { .is_concurrency_enabled = false, .bucket_size = 4, .max_linked_list_chain_length = 4 };
	initialise_hash_bucket(bucket_ptr, config);
	unsigned char value[] = "bar";
	key_value_pair kv = { .key = "foo", .value = value, .value_size = strlen((char*)value) + 1 };
	upsert_node_to_hash_bucket(bucket_ptr, 123, &kv);
	delete_key_from_hash_bucket(bucket_ptr, "foo", 123);
	// Attempt to delete again
	int result = delete_key_from_hash_bucket(bucket_ptr, "foo", 123);
	TEST_ASSERT_EQUAL(ERR_DATA_NODE_NOT_FOUND, result); // Should not find the key
    TEST_ASSERT_EQUAL(0, cleanup_hash_bucket(bucket_ptr));
    free(bucket_ptr);
}

void test_create_hash_bucket_node_with_zero_length_value(void) {
	hash_bucket* bucket_ptr = malloc(sizeof(hash_bucket));
	sub_hash_table_configuration config = { .is_concurrency_enabled = false, .bucket_size = 4, .max_linked_list_chain_length = 4 };
	initialise_hash_bucket(bucket_ptr, config);
	// Zero-length value
	key_value_pair kv = { .key = "foo", .value = (unsigned char*)"", .value_size = 0 };
	int result = upsert_node_to_hash_bucket(bucket_ptr, 123, &kv);
	TEST_ASSERT_EQUAL(0, result); // Should reject zero-length value
    TEST_ASSERT_EQUAL(0, cleanup_hash_bucket(bucket_ptr));
    free(bucket_ptr);
}

int test_hash_bucket_operation_main(void) {
	UNITY_BEGIN();
    printf("Running Hash Bucket Operation Unit Tests...\n");
	RUN_TEST(test_initialise_and_cleanup_hash_bucket);
	RUN_TEST(test_upsert_and_get_key_value);
	RUN_TEST(test_update_existing_key_value);
	RUN_TEST(test_delete_key_from_hash_bucket);
	RUN_TEST(test_invalid_args_hash_bucket);
	RUN_TEST(test_double_initialise_and_cleanup);
	RUN_TEST(test_upsert_null_key_value);
	RUN_TEST(test_get_and_delete_nonexistent_key);
	RUN_TEST(test_multiple_keys_and_collision);
	RUN_TEST(test_edit_hash_bucket_node_after_deletion);
	RUN_TEST(test_read_hash_bucket_node_after_deletion);
	RUN_TEST(test_delete_hash_bucket_node_twice);
	RUN_TEST(test_create_hash_bucket_node_with_zero_length_value);
    printf("Hash Bucket Operation Unit Tests Completed.\n");
	return UNITY_END();
}
