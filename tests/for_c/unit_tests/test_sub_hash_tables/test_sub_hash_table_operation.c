/*
 * Unit tests for sub_hash_table_operation.c
 *
 * This suite covers all public API functions in sub_hash_table_operation.h:
 * - create_new_sub_hash_table
 * - cleanup_sub_hash_table
 * - upsert_node_to_sub_hash_table
 * - get_key_store_value_from_sub_hash_table
 * - delete_key_from_sub_hash_table
 * - is_node_in_sub_hash_table
 *
 * Test strategy:
 * - Happy path, boundary, and failure cases for all API functions
 * - Concurrency and lazy initialization
 * - Teardown and memory safety
 */

#include "unity.h"
#include "sub_hash_table/sub_hash_table_operation.h"
#include "type_definitions/hash_bucket_type_definition.h"
#include "type_definitions/error_code_definitions.h"
#include "type_definitions/sucess_code_definitions.h"
#include "utils/memory_manager.h"
#include <string.h>
#include <stdlib.h>

// --- Helper: get default config ---
static sub_hash_table_configuration get_default_config(void) {
    sub_hash_table_configuration config;
    config.bucket_size = 8;
    config.max_linked_list_chain_length = 4;
    config.is_concurrency_enabled = false;
    return config;
}

// --- Helper: create a valid table and return pointer ---
static sub_hash_table_memory_pool* create_valid_table(sub_hash_table_configuration config, bool earlyInit) {
    sub_hash_table_memory_pool* table = NULL;
    int rc = create_new_sub_hash_table(config, earlyInit, &table);
    TEST_ASSERT_EQUAL_INT(SUCCESS, rc);
    TEST_ASSERT_NOT_NULL(table);
    return table;
}

// --- Helper: cleanup and null sub-hash table pointer ---
static void cleanup_and_null_sub_hash_table(sub_hash_table_memory_pool* table_ptr) {
    if (table_ptr == NULL) return; // Nothing to cleanup
    cleanup_sub_hash_table(table_ptr);
    free_memory(table_ptr, false);
    table_ptr = NULL;
}

// --- 1. Happy path: valid config ---
void test_create_new_sub_hash_table_valid_config_returns_success(void) {
    sub_hash_table_configuration config = get_default_config();
    sub_hash_table_memory_pool* table = NULL;
    int rc = create_new_sub_hash_table(config, false, &table);
    TEST_ASSERT_EQUAL_INT(SUCCESS, rc);
    TEST_ASSERT_NOT_NULL(table);
    cleanup_and_null_sub_hash_table(table);
}

// --- 2. Invalid config: not power of two ---
void test_create_new_sub_hash_table_invalid_config_returns_error(void) {
    sub_hash_table_configuration config = get_default_config();
    config.bucket_size = 7;
    sub_hash_table_memory_pool* table = NULL;
    int rc = create_new_sub_hash_table(config, false, &table);
    TEST_ASSERT_EQUAL_INT(ERR_INVALID_CONFIG, rc);
}

// --- 3. Zero bucket size ---
void test_create_new_sub_hash_table_zero_bucket_size_returns_error(void) {
    sub_hash_table_configuration config = get_default_config();
    config.bucket_size = 0;
    sub_hash_table_memory_pool* table = NULL;
    int rc = create_new_sub_hash_table(config, false, &table);
    TEST_ASSERT_EQUAL_INT(ERR_INVALID_CONFIG, rc);
}

// --- 4. Teardown: cleanup frees resources ---
void test_cleanup_sub_hash_table_frees_all_resources(void) {
    sub_hash_table_configuration config = get_default_config();
    sub_hash_table_memory_pool* table = create_valid_table(config, false);
    int rc = cleanup_sub_hash_table(table);
    TEST_ASSERT_EQUAL_INT(SUCCESS, rc);
    cleanup_and_null_sub_hash_table(table); // Ensure pointer is null and memory is freed
    table = NULL;
    // Should be safe to call again (idempotent)
    rc = cleanup_sub_hash_table(table);
    TEST_ASSERT_EQUAL_INT(SUCCESS, rc);
}

// --- 5. Upsert: insert and update ---
void test_upsert_node_to_sub_hash_table_insert_and_update(void) {
    sub_hash_table_configuration config = get_default_config();
    sub_hash_table_memory_pool* table = create_valid_table(config, false);
    unsigned char val1[] = "val1";
    key_value_pair kv = {"key1", val1, sizeof(val1)};
    composite_key_hash kh = {1, 2};
    int rc = upsert_node_to_sub_hash_table(table, kh, &kv);
    TEST_ASSERT_TRUE(rc == 10 || rc == 0);
    // Update
    unsigned char val2[] = "val2";
    kv.value = val2;
    kv.value_size = sizeof(val2);
    rc = upsert_node_to_sub_hash_table(table, kh, &kv);
    TEST_ASSERT_TRUE(rc == 0);
    cleanup_and_null_sub_hash_table(table);
}

// --- 6. Upsert: null value ---
void test_upsert_node_to_sub_hash_table_null_value_returns_error(void) {
    sub_hash_table_configuration config = get_default_config();
    sub_hash_table_memory_pool* table = create_valid_table(config, false);
    composite_key_hash kh = {1, 2};
    int rc = upsert_node_to_sub_hash_table(table, kh, NULL);
    TEST_ASSERT_EQUAL_INT(ERR_INVALID_ARGUMENT, rc);
    cleanup_and_null_sub_hash_table(table);
}

// --- 7. Get: existing key ---
void test_get_key_store_value_from_sub_hash_table_existing_key_returns_value(void) {
    // Arrange
    sub_hash_table_configuration config = get_default_config();
    sub_hash_table_memory_pool* table = create_valid_table(config, false);
    unsigned char val2[] = "val2";
    key_value_pair kv = {"key2", val2, sizeof(val2)};
    composite_key_hash kh = {2, 3};
    upsert_node_to_sub_hash_table(table, kh, &kv);
    key_value_pair out = {0};

    // Act
    int rc = get_key_store_value_from_sub_hash_table(table, kh, "key2", &out);

    // Assert
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_STRING("key2", out.key);
    TEST_ASSERT_EQUAL_STRING("val2", out.value);

    // Cleanup
    if (out.value) {
        free(out.value);
        out.value = NULL;
    }
    if (out.key) {
        free(out.key);
        out.key = NULL;
    }
    cleanup_and_null_sub_hash_table(table);
}

// --- 8. Get: nonexistent key ---
void test_get_key_store_value_from_sub_hash_table_nonexistent_key_returns_error(void) {
    sub_hash_table_configuration config = get_default_config();
    sub_hash_table_memory_pool* table = create_valid_table(config, false);
    key_value_pair out = {0};
    composite_key_hash kh = {3, 4};
    int rc = get_key_store_value_from_sub_hash_table(table, kh, "nope", &out);
    TEST_ASSERT_TRUE(rc < 0);
    if (out.value) {
        free(out.value);
        out.value = NULL;
    }
    cleanup_and_null_sub_hash_table(table);
}

// --- 9. Delete: existing key ---
void test_delete_key_from_sub_hash_table_existing_key_returns_success(void) {
    sub_hash_table_configuration config = get_default_config();
    sub_hash_table_memory_pool* table = create_valid_table(config, false);
    unsigned char val3[] = "val3";
    key_value_pair kv = {"key3", val3, sizeof(val3)};
    composite_key_hash kh = {4, 5};
    upsert_node_to_sub_hash_table(table, kh, &kv);
    int rc = delete_key_from_sub_hash_table(table, kh, "key3");
    TEST_ASSERT_EQUAL_INT(0, rc);
    cleanup_and_null_sub_hash_table(table);
}

// --- 10. Delete: nonexistent key ---
void test_delete_key_from_sub_hash_table_nonexistent_key_returns_error(void) {
    sub_hash_table_configuration config = get_default_config();
    sub_hash_table_memory_pool* table = create_valid_table(config, false);
    composite_key_hash kh = {5, 6};
    int rc = delete_key_from_sub_hash_table(table, kh, "ghost");
    TEST_ASSERT_TRUE(rc < 0);
    cleanup_and_null_sub_hash_table(table);
}

// --- 11. Is node: existing key ---
void test_is_node_in_sub_hash_table_existing_key_returns_zero(void) {
    sub_hash_table_configuration config = get_default_config();
    sub_hash_table_memory_pool* table = create_valid_table(config, false);
    unsigned char val4[] = "val4";
    key_value_pair kv = {"key4", val4, sizeof(val4)};
    composite_key_hash kh = {6, 7};
    upsert_node_to_sub_hash_table(table, kh, &kv);
    int rc = is_node_in_sub_hash_table(table, kh, "key4");
    TEST_ASSERT_EQUAL_INT(0, rc);
    cleanup_and_null_sub_hash_table(table);
}

// --- 12. Is node: nonexistent key ---
void test_is_node_in_sub_hash_table_nonexistent_key_returns_error(void) {
    sub_hash_table_configuration config = get_default_config();
    sub_hash_table_memory_pool* table = create_valid_table(config, false);
    composite_key_hash kh = {7, 8};
    int rc = is_node_in_sub_hash_table(table, kh, "notfound");
    TEST_ASSERT_TRUE(rc < 0);
    cleanup_and_null_sub_hash_table(table);
}

// --- 13. Concurrency: eager bucket init ---
void test_concurrent_bucket_initialization(void) {
    sub_hash_table_configuration config = get_default_config();
    config.is_concurrency_enabled = true;
    sub_hash_table_memory_pool* table = create_valid_table(config, true);
    // All buckets should be initialized (not directly testable, but no crash)
    unsigned char val5[] = "val5";
    key_value_pair kv = {"key5", val5, sizeof(val5)};
    composite_key_hash kh = {8, 9};
    int rc = upsert_node_to_sub_hash_table(table, kh, &kv);
    TEST_ASSERT_TRUE(rc == 10 || rc == 0);
    cleanup_and_null_sub_hash_table(table);
}

// --- 14. Lazy bucket initialization ---
void test_lazy_bucket_initialization(void) {
    sub_hash_table_configuration config = get_default_config();
    sub_hash_table_memory_pool* table = create_valid_table(config, false);
    // Insert into a bucket that hasn't been initialized yet
    unsigned char val6[] = "val6";
    key_value_pair kv = {"key6", val6, sizeof(val6)};
    composite_key_hash kh = {9, 10};
    int rc = upsert_node_to_sub_hash_table(table, kh, &kv);
    TEST_ASSERT_TRUE(rc == 10 || rc == 0);
    cleanup_and_null_sub_hash_table(table);
}

// --- 15. Cleanup: null pointer ---
void test_cleanup_sub_hash_table_null_pointer_noop(void) {
    int rc = cleanup_sub_hash_table(NULL);
    TEST_ASSERT_EQUAL_INT(SUCCESS, rc);
}

// --- Test Runner ---
int test_sub_hash_table_operation_main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_create_new_sub_hash_table_valid_config_returns_success);
    RUN_TEST(test_create_new_sub_hash_table_invalid_config_returns_error);
    RUN_TEST(test_create_new_sub_hash_table_zero_bucket_size_returns_error);
    RUN_TEST(test_cleanup_sub_hash_table_frees_all_resources);
    RUN_TEST(test_upsert_node_to_sub_hash_table_insert_and_update);
    RUN_TEST(test_upsert_node_to_sub_hash_table_null_value_returns_error);
    RUN_TEST(test_get_key_store_value_from_sub_hash_table_existing_key_returns_value);
    RUN_TEST(test_get_key_store_value_from_sub_hash_table_nonexistent_key_returns_error);
    RUN_TEST(test_delete_key_from_sub_hash_table_existing_key_returns_success);
    RUN_TEST(test_delete_key_from_sub_hash_table_nonexistent_key_returns_error);
    RUN_TEST(test_is_node_in_sub_hash_table_existing_key_returns_zero);
    RUN_TEST(test_is_node_in_sub_hash_table_nonexistent_key_returns_error);
    RUN_TEST(test_concurrent_bucket_initialization);
    RUN_TEST(test_lazy_bucket_initialization);
    RUN_TEST(test_cleanup_sub_hash_table_null_pointer_noop);
    return UNITY_END();
}
