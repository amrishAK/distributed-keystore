
#include "unity.h"
#include "hash_table/hash_table_operation.h"
#include "type_definitions/hash_bucket_type_definition.h"
#include "type_definitions/custom_type_definitions.h"
#include "type_definitions/config_type_definitions.h"
#include "type_definitions/error_code_definitions.h"
#include <string.h>
#include <stdlib.h>

// --- Helper Functions ---
static hash_table_configuration valid_config(void) {
	hash_table_configuration config = {
		.bucket_size = 8,
		.is_concurrency_enabled = false,
		.sub_hash_table_bucket_size = 4,
		.max_linked_list_chain_length = 8
	};
	return config;
}

static composite_key_hash make_key_hash(uint64_t bucket, uint64_t sub) {
	composite_key_hash h = { .bucket_hash = bucket, .sub_bucket_hash = sub };
	return h;
}

static key_value_pair make_kv_hash_table_op(const char* key, const char* value) {
	key_value_pair kv;
	kv.key = (char*)key;
	kv.value = (unsigned char*)value;
	kv.value_size = strlen(value)+1; // Include null terminator
	return kv;
}

// --- Cleanup Helper ---
static void cleanup_table(hash_table_memory_pool* table) {
	if (table != NULL) {
		// Always attempt to clean up the hash table (frees sub-allocations if initialized)
		cleanup_hash_table(table);
		// Free hash_buckets_ptr if still allocated (e.g., if not initialized or cleanup_hash_table is a no-op)
		if (table->hash_buckets_ptr != NULL) {
			free_memory(table->hash_buckets_ptr, false);
			table->hash_buckets_ptr = NULL;
		}
		// Free the table struct itself
		free_memory(table, false);
	}
}

static void cleanup_kv_pair(key_value_pair* kv) {
	if (kv == NULL) return;
	if (kv->key) free_memory(kv->key, false);
	if (kv->value) free_memory(kv->value, false);
}


// --- Tests ---

void test_create_new_hash_table_valid_config_returns_success(void) {
	// Arrange
	hash_table_memory_pool* table = NULL;
	hash_table_configuration config = valid_config();

	// Act
	int result = create_new_hash_table(config, &table);

	// Assert
	TEST_ASSERT_EQUAL_INT(0, result);
	TEST_ASSERT_NOT_NULL(table);
	TEST_ASSERT_TRUE(table->is_initialized);
	TEST_ASSERT_EQUAL(config.bucket_size, table->total_blocks);
	cleanup_table(table);
}

void test_create_new_hash_table_null_output_pointer_returns_error(void) {
	// Arrange
	hash_table_configuration config = valid_config();

	// Act
	int result = create_new_hash_table(config, NULL);

	// Assert
	TEST_ASSERT_EQUAL_INT(ERR_INVALID_ARGUMENT, result);
}

void test_create_new_hash_table_zero_bucket_size_returns_error(void) {
	// Arrange
	hash_table_memory_pool* table = NULL;
	hash_table_configuration config = valid_config();
	config.bucket_size = 0;

	// Act
	int result = create_new_hash_table(config, &table);

	// Assert
	TEST_ASSERT_EQUAL_INT(ERR_INVALID_CONFIG, result);
	cleanup_table(table);
}

void test_create_new_hash_table_non_power_of_two_bucket_size_returns_error(void) {
	// Arrange
	hash_table_memory_pool* table = NULL;
	hash_table_configuration config = valid_config();
	config.bucket_size = 7; // not a power of two

	// Act
	int result = create_new_hash_table(config, &table);

	// Assert
	TEST_ASSERT_EQUAL_INT(ERR_INVALID_CONFIG, result);
	cleanup_table(table);
}

void test_cleanup_hash_table_valid_table_frees_memory(void) {
	// Arrange
	hash_table_memory_pool* table = NULL;
	hash_table_configuration config = valid_config();
	int result = create_new_hash_table(config, &table);
	TEST_ASSERT_EQUAL_INT(0, result);

	// Act
	result = cleanup_hash_table(table);

	// Assert
	TEST_ASSERT_EQUAL_INT(0, result);
	TEST_ASSERT_FALSE(table->is_initialized);
	cleanup_table(table);
}

void test_cleanup_hash_table_null_pointer_noop(void) {
	// Act
	int result = cleanup_hash_table(NULL);

	// Assert
	TEST_ASSERT_EQUAL_INT(0, result);
}

void test_cleanup_hash_table_uninitialized_noop(void) {
	// Arrange
	hash_table_memory_pool* table = NULL;
	hash_table_configuration config = valid_config();
	int result = create_new_hash_table(config, &table);
	TEST_ASSERT_EQUAL_INT(0, result);
	table->is_initialized = false;

	// Act
	result = cleanup_hash_table(table);

	// Assert
	TEST_ASSERT_EQUAL_INT(0, result);
	cleanup_table(table);
}

void test_upsert_node_to_hash_table_valid_inserts_success(void) {
	// Arrange
	hash_table_memory_pool* table = NULL;
	hash_table_configuration config = valid_config();
	create_new_hash_table(config, &table);
	key_value_pair kv = make_kv_hash_table_op("foo", "bar");
	composite_key_hash kh = make_key_hash(1, 2);

	// Act
	int result = upsert_node_to_hash_table(table, kh, &kv);

	// Assert
	TEST_ASSERT_TRUE(result == 0 || result == 10 || result == 20);
	cleanup_table(table);
}

void test_upsert_node_to_hash_table_null_kv_pair_returns_error(void) {
	// Arrange
	hash_table_memory_pool* table = NULL;
	hash_table_configuration config = valid_config();
	create_new_hash_table(config, &table);
	composite_key_hash kh = make_key_hash(1, 2);

	// Act
	int result = upsert_node_to_hash_table(table, kh, NULL);

	// Assert
	TEST_ASSERT_EQUAL_INT(ERR_INVALID_ARGUMENT, result);
	cleanup_table(table);
}

void test_upsert_node_to_hash_table_invalid_bucket_returns_error(void) {
	// Arrange
	key_value_pair kv = make_kv_hash_table_op("foo", "bar");
	composite_key_hash kh = make_key_hash(0, 2); // bucket_hash 0 triggers error

	// Act
	int result = upsert_node_to_hash_table(NULL, kh, &kv);

	// Assert
	TEST_ASSERT_EQUAL_INT(ERR_INVALID_ARGUMENT, result);
}

void test_get_key_value_from_hash_table_valid_returns_success(void) {
	// Arrange
	hash_table_memory_pool* table = NULL;
	hash_table_configuration config = valid_config();
	create_new_hash_table(config, &table);
	key_value_pair kv = make_kv_hash_table_op("foo", "bar");
	composite_key_hash kh = make_key_hash(1, 2);
	upsert_node_to_hash_table(table, kh, &kv);
	key_value_pair out = {0};

	// Act
	int result = get_key_value_from_hash_table(table, kh, "foo", &out);

	// Assert
	TEST_ASSERT_EQUAL_INT(0, result);
	TEST_ASSERT_EQUAL_STRING("foo", out.key);
	TEST_ASSERT_EQUAL_STRING("bar", (char*)out.value);
    cleanup_kv_pair(&out);
    cleanup_table(table);
}

void test_get_key_value_from_hash_table_null_output_returns_error(void) {
	// Arrange
	hash_table_memory_pool* table = NULL;
	hash_table_configuration config = valid_config();
	create_new_hash_table(config, &table);
	key_value_pair kv = make_kv_hash_table_op("foo", "bar");
	composite_key_hash kh = make_key_hash(1, 2);
	upsert_node_to_hash_table(table, kh, &kv);

	// Act
	int result = get_key_value_from_hash_table(table, kh, "foo", NULL);

	// Assert
	TEST_ASSERT_EQUAL_INT(ERR_INVALID_ARGUMENT, result);
	cleanup_table(table);
}

void test_get_key_value_from_hash_table_invalid_bucket_returns_error(void) {
	// Arrange
	key_value_pair out = {0};
	composite_key_hash kh = make_key_hash(0, 2); // bucket_hash 0 triggers error

	// Act
	int result = get_key_value_from_hash_table(NULL, kh, "foo", &out);

	// Assert
	TEST_ASSERT_EQUAL_INT(ERR_INVALID_ARGUMENT, result);
}

void test_delete_key_from_hash_table_valid_deletes_success(void) {
	// Arrange
	hash_table_memory_pool* table = NULL;
	hash_table_configuration config = valid_config();
	create_new_hash_table(config, &table);
	key_value_pair kv = make_kv_hash_table_op("foo", "bar");
	composite_key_hash kh = make_key_hash(1, 2);
	upsert_node_to_hash_table(table, kh, &kv);

	// Act
	int result = delete_key_from_hash_table(table, kh, "foo");

	// Assert
	TEST_ASSERT_EQUAL_INT(0, result);
	cleanup_table(table);
}

void test_delete_key_from_hash_table_null_key_returns_error(void) {
	// Arrange
	hash_table_memory_pool* table = NULL;
	hash_table_configuration config = valid_config();
	create_new_hash_table(config, &table);
	composite_key_hash kh = make_key_hash(1, 2);

	// Act
	int result = delete_key_from_hash_table(table, kh, NULL);

	// Assert
	TEST_ASSERT_EQUAL_INT(ERR_INVALID_ARGUMENT, result);
	cleanup_table(table);
}

void test_delete_key_from_hash_table_invalid_bucket_returns_error(void) {
	// Arrange
	composite_key_hash kh = make_key_hash(0, 2); // bucket_hash 0 triggers error

	// Act
	int result = delete_key_from_hash_table(NULL, kh, "foo");

	// Assert
	TEST_ASSERT_EQUAL_INT(ERR_INVALID_ARGUMENT, result);
}

void test_concurrent_bucket_initialization_hash_table_op(void) {
	// Arrange
	hash_table_memory_pool* table = NULL;
	hash_table_configuration config = valid_config();
	config.is_concurrency_enabled = true;

	// Act
	int result = create_new_hash_table(config, &table);

	// Assert
	TEST_ASSERT_EQUAL_INT(0, result);
	for (unsigned int i = 0; i < table->total_blocks; ++i) {
		TEST_ASSERT_TRUE(table->hash_buckets_ptr[i].is_initialized);
	}
	cleanup_table(table);
}

void test_lazy_bucket_initialization_on_first_access(void) {
	// Arrange
	hash_table_memory_pool* table = NULL;
	hash_table_configuration config = valid_config();
	config.is_concurrency_enabled = false;
	int result = create_new_hash_table(config, &table);
	TEST_ASSERT_EQUAL_INT(0, result);

	// Assert (before access)
	for (unsigned int i = 0; i < table->total_blocks; ++i) {
		TEST_ASSERT_FALSE(table->hash_buckets_ptr[i].is_initialized);
	}

	// Act
	key_value_pair kv = make_kv_hash_table_op("foo", "bar");
	composite_key_hash kh = make_key_hash(1, 2);
	upsert_node_to_hash_table(table, kh, &kv);

	// Assert (after access)
	int initialized_count = 0;
	for (unsigned int i = 0; i < table->total_blocks; ++i) {
		if (table->hash_buckets_ptr[i].is_initialized) initialized_count++;
	}
	TEST_ASSERT_TRUE(initialized_count > 0);
	cleanup_table(table);
}

void test_cleanup_hash_table_idempotency(void) {
	// Arrange
	hash_table_memory_pool* table = NULL;
	hash_table_configuration config = valid_config();
	int result = create_new_hash_table(config, &table);
	TEST_ASSERT_EQUAL_INT(0, result);

	// Act & Assert
	result = cleanup_hash_table(table);
	TEST_ASSERT_EQUAL_INT(0, result);
	// Call cleanup again
	result = cleanup_hash_table(table);
	TEST_ASSERT_EQUAL_INT(0, result);
	cleanup_table(table);
}

int test_hash_table_operation_main(void) {
	UNITY_BEGIN();
	RUN_TEST(test_create_new_hash_table_valid_config_returns_success);
	RUN_TEST(test_create_new_hash_table_null_output_pointer_returns_error);
	RUN_TEST(test_create_new_hash_table_zero_bucket_size_returns_error);
	RUN_TEST(test_create_new_hash_table_non_power_of_two_bucket_size_returns_error);
	RUN_TEST(test_cleanup_hash_table_valid_table_frees_memory);
	RUN_TEST(test_cleanup_hash_table_null_pointer_noop);
	RUN_TEST(test_cleanup_hash_table_uninitialized_noop);
	RUN_TEST(test_upsert_node_to_hash_table_valid_inserts_success);
	RUN_TEST(test_upsert_node_to_hash_table_null_kv_pair_returns_error);
	RUN_TEST(test_upsert_node_to_hash_table_invalid_bucket_returns_error);
	RUN_TEST(test_get_key_value_from_hash_table_valid_returns_success);
	RUN_TEST(test_get_key_value_from_hash_table_null_output_returns_error);
	RUN_TEST(test_get_key_value_from_hash_table_invalid_bucket_returns_error);
	RUN_TEST(test_delete_key_from_hash_table_valid_deletes_success);
	RUN_TEST(test_delete_key_from_hash_table_null_key_returns_error);
	RUN_TEST(test_delete_key_from_hash_table_invalid_bucket_returns_error);
	RUN_TEST(test_concurrent_bucket_initialization_hash_table_op);
	RUN_TEST(test_lazy_bucket_initialization_on_first_access);
	RUN_TEST(test_cleanup_hash_table_idempotency);
	return UNITY_END();
}
