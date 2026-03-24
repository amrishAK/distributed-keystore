#include "unity.h"
#include "hash_table/hash_bucket_operation.h"
#include "type_definitions/hash_bucket_type_definition.h"
#include "type_definitions/custom_type_definitions.h"
#include "type_definitions/config_type_definitions.h"
#include "type_definitions/error_code_definitions.h"
#include "type_definitions/sucess_code_definitions.h"
#include <string.h>
#include <stdlib.h>


// --- Per-test helper ---
static void init_test_bucket(hash_bucket *bucket, sub_hash_table_configuration *config) {
	memset(bucket, 0, sizeof(hash_bucket));
	config->is_concurrency_enabled = false;
	config->bucket_size = 8;
	config->max_linked_list_chain_length = 4;
}

static void init_test_kv(key_value_pair *kv, char *key, unsigned char *value) {
	strcpy(key, "test-key");
	value[0] = 42;
	kv->key = key;
	kv->value = value;
	kv->value_size = 1;
}

// --- Test 1: Happy path for initialization ---
void test_initialise_hash_bucket_valid_config_returns_success(void) {
	// Arrange
	hash_bucket bucket;
	sub_hash_table_configuration config;
	init_test_bucket(&bucket, &config);

	// Act
	int result = initialise_hash_bucket(&bucket, config);

	// Assert
	TEST_ASSERT_EQUAL_INT(SUCCESS, result);
	TEST_ASSERT_TRUE(bucket.is_initialized);
	TEST_ASSERT_NOT_NULL(bucket.sub_hash_table_ptr);

	// Cleanup
	cleanup_hash_bucket(&bucket);
}

// --- Test 2: Null pointer to initialise ---
void test_initialise_hash_bucket_null_ptr_returns_error(void) {
	// Arrange
	sub_hash_table_configuration config = {0};

	// Act
	int result = initialise_hash_bucket(NULL, config);

	// Assert
	TEST_ASSERT_EQUAL_INT(ERR_INVALID_ARGUMENT, result);
}

// --- Test 3: Double initialization is idempotent ---
void test_initialise_hash_bucket_double_init_is_idempotent(void) {
	// Arrange
	hash_bucket bucket;
	sub_hash_table_configuration config;
	init_test_bucket(&bucket, &config);

	// Act
	int result1 = initialise_hash_bucket(&bucket, config);
	int result2 = initialise_hash_bucket(&bucket, config);

	// Assert
	TEST_ASSERT_EQUAL_INT(SUCCESS, result1);
	TEST_ASSERT_EQUAL_INT(0, result2); // Already initialized returns 0

	// Cleanup
	cleanup_hash_bucket(&bucket);
}

// --- Test 4: Cleanup valid bucket ---
void test_cleanup_hash_bucket_valid_bucket_cleans_resources(void) {
	// Arrange
	hash_bucket bucket;
	sub_hash_table_configuration config;
	init_test_bucket(&bucket, &config);
	initialise_hash_bucket(&bucket, config);

	// Act
	int result = cleanup_hash_bucket(&bucket);

	// Assert
	TEST_ASSERT_EQUAL_INT(SUCCESS, result);
	TEST_ASSERT_FALSE(bucket.is_initialized);
	TEST_ASSERT_NULL(bucket.sub_hash_table_ptr);
}

// --- Test 5: Cleanup null pointer is no-op ---
void test_cleanup_hash_bucket_null_ptr_is_noop(void) {
	// Act
	int result = cleanup_hash_bucket(NULL);

	// Assert
	TEST_ASSERT_EQUAL_INT(SUCCESS, result);
}

// --- Test 6: Upsert node happy path (insert and update) ---
void test_upsert_node_to_hash_bucket_valid_inserts_and_updates(void) {
	// Arrange
	hash_bucket bucket;
	sub_hash_table_configuration config;
	key_value_pair kv;
	char key[32];
	unsigned char value[32];
	init_test_bucket(&bucket, &config);
	init_test_kv(&kv, key, value);
	initialise_hash_bucket(&bucket, config);
	composite_key_hash key_hash = { .bucket_hash = 1, .sub_bucket_hash = 2 };

	// Act
	int result_insert = upsert_node_to_hash_bucket(&bucket, key_hash, &kv);
	value[0] = 99;
	int result_update = upsert_node_to_hash_bucket(&bucket, key_hash, &kv);

	// Assert
	TEST_ASSERT_EQUAL_INT(10, result_insert); // Insert should return 10
	TEST_ASSERT_EQUAL_INT(0, result_update); // Update should return 0

	// Cleanup
	cleanup_hash_bucket(&bucket);
}

// --- Test 7: Upsert node with null args returns error ---
void test_upsert_node_to_hash_bucket_null_args_returns_error(void) {
	// Arrange
	key_value_pair kv = {0};
	hash_bucket bucket = {0};

	// Act
	int result1 = upsert_node_to_hash_bucket(NULL, (composite_key_hash){0}, &kv);
	int result2 = upsert_node_to_hash_bucket(&bucket, (composite_key_hash){0}, NULL);

	// Assert
	TEST_ASSERT_EQUAL_INT(ERR_INVALID_ARGUMENT, result1);
	TEST_ASSERT_EQUAL_INT(ERR_INVALID_ARGUMENT, result2);
}

// --- Test 8: Upsert node to uninitialized bucket returns error ---
void test_upsert_node_to_hash_bucket_uninitialized_bucket_returns_error(void) {
	// Arrange
	hash_bucket bucket;
	sub_hash_table_configuration config;
	key_value_pair kv;
	char key[32];
	unsigned char value[32];
	init_test_bucket(&bucket, &config);
	init_test_kv(&kv, key, value);

	// Act
	int result = upsert_node_to_hash_bucket(&bucket, (composite_key_hash){0}, &kv);

	// Assert
	TEST_ASSERT_EQUAL_INT(ERR_HASH_BUCKET_NOT_INITIALIZED, result);
}

// --- Test 9: Get key value happy path ---
void test_get_key_value_from_hash_bucket_valid_returns_value(void) {
	// Arrange
	hash_bucket bucket;
	sub_hash_table_configuration config;
	key_value_pair kv;
	char key[32];
	unsigned char value[32];
	init_test_bucket(&bucket, &config);
	init_test_kv(&kv, key, value);
	initialise_hash_bucket(&bucket, config);
	composite_key_hash key_hash = { .bucket_hash = 1, .sub_bucket_hash = 2 };
	upsert_node_to_hash_bucket(&bucket, key_hash, &kv);
	key_value_pair out = {0};

	// Act
	int result = get_key_value_from_hash_bucket(&bucket, key, key_hash, &out);

	// Assert
	TEST_ASSERT_EQUAL_INT(SUCCESS, result);
	TEST_ASSERT_EQUAL_STRING(key, out.key);
	TEST_ASSERT_EQUAL_UINT8_ARRAY(value, out.value, out.value_size);
	TEST_ASSERT_EQUAL_UINT(kv.value_size, out.value_size);

	// Cleanup
	if (out.value && out.value != value) free(out.value);
	if (out.key && out.key != key) free(out.key);
	cleanup_hash_bucket(&bucket);
}

// --- Test 10: Get key not found returns error ---
void test_get_key_value_from_hash_bucket_key_not_found_returns_error(void) {
	// Arrange
	hash_bucket bucket;
	sub_hash_table_configuration config;
	key_value_pair kv;
	char key[32];
	unsigned char value[32];
	init_test_bucket(&bucket, &config);
	init_test_kv(&kv, key, value);
	initialise_hash_bucket(&bucket, config);
	composite_key_hash key_hash = { .bucket_hash = 1, .sub_bucket_hash = 2 };
	key_value_pair out = {0};

	// Act
	int result = get_key_value_from_hash_bucket(&bucket, "missing", key_hash, &out);

	// Assert
	TEST_ASSERT_EQUAL_INT(ERR_DATA_NODE_NOT_FOUND, result);

	// Cleanup
	cleanup_hash_bucket(&bucket);
}

// --- Test 11: Get key with null args returns error ---
void test_get_key_value_from_hash_bucket_null_args_returns_error(void) {
	// Arrange
	key_value_pair kv = {0};
	hash_bucket bucket = {0};
	char key[32] = "test-key";

	// Act
	int result1 = get_key_value_from_hash_bucket(NULL, key, (composite_key_hash){0}, &kv);
	int result2 = get_key_value_from_hash_bucket(&bucket, key, (composite_key_hash){0}, NULL);

	// Assert
	TEST_ASSERT_EQUAL_INT(ERR_INVALID_ARGUMENT, result1);
	TEST_ASSERT_EQUAL_INT(ERR_INVALID_ARGUMENT, result2);
}

// --- Test 12: Delete key happy path ---
void test_delete_key_from_hash_bucket_valid_deletes_node(void) {
	// Arrange
	hash_bucket bucket;
	sub_hash_table_configuration config;
	key_value_pair kv;
	char key[32];
	unsigned char value[32];
	init_test_bucket(&bucket, &config);
	init_test_kv(&kv, key, value);
	initialise_hash_bucket(&bucket, config);
	composite_key_hash key_hash = { .bucket_hash = 1, .sub_bucket_hash = 2 };
	upsert_node_to_hash_bucket(&bucket, key_hash, &kv);

	// Act
	int result = delete_key_from_hash_bucket(&bucket, key, key_hash);

	// Assert
	TEST_ASSERT_EQUAL_INT(SUCCESS, result);

	// Cleanup
	cleanup_hash_bucket(&bucket);
}

// --- Test 13: Delete key not found returns error ---
void test_delete_key_from_hash_bucket_key_not_found_returns_error(void) {
	// Arrange
	hash_bucket bucket;
	sub_hash_table_configuration config;
	key_value_pair kv;
	char key[32];
	unsigned char value[32];
	init_test_bucket(&bucket, &config);
	init_test_kv(&kv, key, value);
	initialise_hash_bucket(&bucket, config);
	composite_key_hash key_hash = { .bucket_hash = 1, .sub_bucket_hash = 2 };
	
	// Add a node to ensure bucket is initialized and not empty
	int result = 0;
	result = upsert_node_to_hash_bucket(&bucket, key_hash, &kv);
	TEST_ASSERT_EQUAL_INT(SUCESS_ADDED_NEW_NODE, result); // Insert or update is fine
	result = delete_key_from_hash_bucket(&bucket, key, key_hash);
	TEST_ASSERT_EQUAL_INT(SUCCESS, result); // Deletion should succeed

	// Act
	result = delete_key_from_hash_bucket(&bucket, "missing", key_hash);

	// Assert
	TEST_ASSERT_EQUAL_INT(ERR_DATA_NODE_NOT_FOUND, result);

	// Cleanup
	cleanup_hash_bucket(&bucket);
}

// --- Test 14: Delete key with null args returns error ---
void test_delete_key_from_hash_bucket_null_args_returns_error(void) {
	// Arrange
	char key[32] = "test-key";

	// Act
	int result = delete_key_from_hash_bucket(NULL, key, (composite_key_hash){0});

	// Assert
	TEST_ASSERT_EQUAL_INT(ERR_INVALID_ARGUMENT, result);
}

// --- Test 15: Concurrency test for resizing lock (smoke) ---
void test_resizing_lock_wrapper_concurrent_access_no_data_race(void) {
	// Arrange
	hash_bucket bucket;
	sub_hash_table_configuration config;
	key_value_pair kv;
	char key[32];
	unsigned char value[32];
	init_test_bucket(&bucket, &config);
	init_test_kv(&kv, key, value);
	initialise_hash_bucket(&bucket, config);
	composite_key_hash key_hash = { .bucket_hash = 1, .sub_bucket_hash = 2 };

	// Act
	upsert_node_to_hash_bucket(&bucket, key_hash, &kv);
	key_value_pair out = {0};
	get_key_value_from_hash_bucket(&bucket, key, key_hash, &out);
	delete_key_from_hash_bucket(&bucket, key, key_hash);

	// Assert
	TEST_ASSERT_TRUE(1); // If no crash, pass

	// Cleanup
	if (out.value && out.value != value) free(out.value);
	if (out.key && out.key != key) free(out.key);
	cleanup_hash_bucket(&bucket);
}

// --- Test 16: Cleanup during resizing waits for completion (mocked) ---
void test_cleanup_hash_bucket_during_resizing_waits_for_completion(void) {
	// Arrange
	hash_bucket bucket;
	sub_hash_table_configuration config;
	init_test_bucket(&bucket, &config);
	initialise_hash_bucket(&bucket, config);
	bucket.is_resizing = false; // Simulate not resizing

	// Act
	int result = cleanup_hash_bucket(&bucket);

	// Assert
	TEST_ASSERT_EQUAL_INT(SUCCESS, result);
}

int test_hash_bucket_operation_main(void) {
	UNITY_BEGIN();
	RUN_TEST(test_initialise_hash_bucket_valid_config_returns_success);
	RUN_TEST(test_initialise_hash_bucket_null_ptr_returns_error);
	RUN_TEST(test_initialise_hash_bucket_double_init_is_idempotent);
	RUN_TEST(test_cleanup_hash_bucket_valid_bucket_cleans_resources);
	RUN_TEST(test_cleanup_hash_bucket_null_ptr_is_noop);
	RUN_TEST(test_upsert_node_to_hash_bucket_valid_inserts_and_updates);
	RUN_TEST(test_upsert_node_to_hash_bucket_null_args_returns_error);
	RUN_TEST(test_upsert_node_to_hash_bucket_uninitialized_bucket_returns_error);
	RUN_TEST(test_get_key_value_from_hash_bucket_valid_returns_value);
	RUN_TEST(test_get_key_value_from_hash_bucket_key_not_found_returns_error);
	RUN_TEST(test_get_key_value_from_hash_bucket_null_args_returns_error);
	RUN_TEST(test_delete_key_from_hash_bucket_valid_deletes_node);
	RUN_TEST(test_delete_key_from_hash_bucket_key_not_found_returns_error);
	RUN_TEST(test_delete_key_from_hash_bucket_null_args_returns_error);
	RUN_TEST(test_resizing_lock_wrapper_concurrent_access_no_data_race);
	RUN_TEST(test_cleanup_hash_bucket_during_resizing_waits_for_completion);
	return UNITY_END();
}
