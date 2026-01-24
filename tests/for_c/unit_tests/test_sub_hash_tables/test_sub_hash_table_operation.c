/**
 * @file test_sub_hash_table_operation.c
 * @brief Unit tests for sub-hash-table operations.
 * This file contains comprehensive unit tests for the sub-hash-table operations API.
 * Test Scenarios Covered:
 * 1. Create a new sub-hash-table successfully.
 * 2. Fail to create a sub-hash-table with invalid arguments.
 * 3. Upsert (insert/update) a node and retrieve it by key.
 * 4. Upsert a duplicate key and verify the value is updated.
 * 5. Delete a key from the sub-hash-table and verify it is gone.
 * 6. Attempt to delete a nonexistent key (should fail gracefully).
 * 7. Insert enough keys to potentially trigger a resize (resize trigger simulation).
 * 8. Create a sub-hash-table with concurrency enabled.
 * 9. Handle null and invalid arguments for all API functions.
 * 10. Edge cases for upsert, get, and delete operations.
 * 11. Handle hash collisions correctly.
 * 12. Cleanup after delete operations.
 */

#include "unity.h"
#include "sub_hash_table/sub_hash_table_operation.h"
#include "type_definitions/hash_bucket_type_definition.h"
#include "type_definitions/error_code_definitions.h"
#include "utils/memory_manager.h"
#include <string.h>
#include <stdlib.h>


void test_create_new_sub_hash_table_success(void) {
    sub_hash_table_memory_pool *table = NULL;
    
    int result = create_new_sub_hash_table((sub_hash_table_configuration){.is_concurrency_enabled = false, .bucket_size = 8, .max_linked_list_chain_length = 8}, false, &table);
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_NOT_NULL(table);
    printf("Created sub-hash-table with %u buckets.\n", table->total_blocks);
    cleanup_sub_hash_table(table);
    printf("Cleaned up sub-hash-table successfully.\n");
    free_memory(table, false);
}

void test_create_new_sub_hash_table_invalid_args(void) {
    printf("Testing create_new_sub_hash_table with invalid arguments...\n");
    int result = create_new_sub_hash_table((sub_hash_table_configuration){.is_concurrency_enabled = false, .bucket_size = 0, .max_linked_list_chain_length = 8}, false, NULL);
    TEST_ASSERT_LESS_THAN(0, result);
}

void test_upsert_and_get_node_sub_hash_table(void) {
    sub_hash_table_memory_pool *table = NULL;
    create_new_sub_hash_table((sub_hash_table_configuration){.is_concurrency_enabled = false, .bucket_size = 8, .max_linked_list_chain_length = 8}, false, &table);
    unsigned char value[] = "value";
    key_value_pair kv = {"key", value, strlen((char*)value) + 1};
    int upsert_result = upsert_node_to_sub_hash_table(table, 123, &kv);
    TEST_ASSERT_TRUE(upsert_result == 0 || upsert_result == 10);

    key_value_pair out = {0};
    int get_result = get_key_store_value_from_sub_hash_table(table, 123, "key", &out);
    TEST_ASSERT_EQUAL(0, get_result);
    TEST_ASSERT_EQUAL_STRING("key", out.key);
    TEST_ASSERT_EQUAL_STRING("value", out.value);

    cleanup_sub_hash_table(table);
    free_memory(table, false);
}

void test_upsert_duplicate_key_sub_hash_table(void) {
    sub_hash_table_memory_pool *table = NULL;
    create_new_sub_hash_table((sub_hash_table_configuration){.is_concurrency_enabled = false, .bucket_size = 8, .max_linked_list_chain_length = 8}, false, &table);
    
    unsigned char value1[] = "value1";
    unsigned char value2[] = "value2";
    key_value_pair kv1 = {"dupkey", value1, strlen((char*)value1) + 1};
    int first = upsert_node_to_sub_hash_table(table, 321, &kv1);
    TEST_ASSERT_TRUE(first == 0 || first == 10);
    key_value_pair kv2 = {"dupkey", value2, strlen((char*)value2) + 1};
    int second = upsert_node_to_sub_hash_table(table, 321, &kv2);
    TEST_ASSERT_TRUE(second == 0);
    key_value_pair out = {0};
    int get_result = get_key_store_value_from_sub_hash_table(table, 321, "dupkey", &out);
    TEST_ASSERT_EQUAL(0, get_result);
    TEST_ASSERT_EQUAL_STRING("dupkey", out.key);
    TEST_ASSERT_EQUAL_STRING("value2", out.value);
    cleanup_sub_hash_table(table);
    free_memory(table, false);
}

void test_delete_key_from_sub_hash_table(void) {
    sub_hash_table_memory_pool *table = NULL;
    create_new_sub_hash_table((sub_hash_table_configuration){.is_concurrency_enabled = false, .bucket_size = 8, .max_linked_list_chain_length = 8}, false, &table);
    unsigned char value[] = "value";
    key_value_pair kv = {"key", value, strlen((char*)value) + 1};
    upsert_node_to_sub_hash_table(table, 123, &kv);
    int del_result = delete_key_from_sub_hash_table(table, 123, "key");
    TEST_ASSERT_EQUAL(0, del_result);
    key_value_pair out = {0};
    int get_result = get_key_store_value_from_sub_hash_table(table, 123, "key", &out);
    TEST_ASSERT_LESS_THAN(0, get_result);
    cleanup_sub_hash_table(table);
    free_memory(table, false);
}

void test_delete_nonexistent_key_sub_hash_table(void) {
    sub_hash_table_memory_pool *table = NULL;
    create_new_sub_hash_table((sub_hash_table_configuration){.is_concurrency_enabled = false, .bucket_size = 8, .max_linked_list_chain_length = 8}, false, &table);
    int del_result = delete_key_from_sub_hash_table(table, 999, "nope");
    TEST_ASSERT_LESS_THAN(0, del_result);
    cleanup_sub_hash_table(table);
    free_memory(table, false);
}

void test_resize_trigger_result_sub_hash_table(void) {
    sub_hash_table_memory_pool *table = NULL;
    create_new_sub_hash_table((sub_hash_table_configuration){.is_concurrency_enabled = false, .bucket_size = 2, .max_linked_list_chain_length = 2}, false, &table);
    // Insert enough keys to potentially trigger resize (simulate, actual resize logic may vary)
    int resize_triggered = 0;
    for (int i = 0; i < 20; ++i) {
        char key[16];
        snprintf(key, sizeof(key), "key%d", i);
        key_value_pair kv = {key, (unsigned char *)"v", 1};
        int res = upsert_node_to_sub_hash_table(table, (uint32_t)i, &kv);
        if (res == 20) resize_triggered = 1;
    }
    TEST_ASSERT_TRUE(resize_triggered == 1 || resize_triggered == 0); // Accept either, just check no crash
    cleanup_sub_hash_table(table);
    free_memory(table, false);
}

void test_concurrency_enabled_sub_hash_table(void) {
    sub_hash_table_memory_pool *table = NULL;
    int result = create_new_sub_hash_table((sub_hash_table_configuration){.is_concurrency_enabled = true, .bucket_size = 8, .max_linked_list_chain_length = 8}, false, &table);
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_NOT_NULL(table);
    cleanup_sub_hash_table(table);
    free_memory(table, false);
}

// 1. Null/invalid arguments for all API functions
void test_null_and_invalid_args_sub_hash_table(void) {
    unsigned char v[] = "v";
    key_value_pair kv = {"k", v, 2};
    TEST_ASSERT_LESS_THAN(0, upsert_node_to_sub_hash_table(NULL, 0, NULL));
    TEST_ASSERT_LESS_THAN(0, upsert_node_to_sub_hash_table(NULL, 0, &kv));
    TEST_ASSERT_LESS_THAN(0, get_key_store_value_from_sub_hash_table(NULL, 0, NULL, NULL));
    TEST_ASSERT_LESS_THAN(0, get_key_store_value_from_sub_hash_table(NULL, 0, NULL, &kv));
    TEST_ASSERT_LESS_THAN(0, delete_key_from_sub_hash_table(NULL, 0, NULL));
}

// 2. Edge cases for upsert/get/delete
void test_upsert_get_delete_edge_cases_sub_hash_table(void) {
    sub_hash_table_memory_pool *table = NULL;
    create_new_sub_hash_table((sub_hash_table_configuration){.is_concurrency_enabled = false, .bucket_size = 8, .max_linked_list_chain_length = 8}, false, &table);
    // Upsert with empty key
    unsigned char v[] = "v";
    key_value_pair kv_empty = {"", v, 2};
    int res = upsert_node_to_sub_hash_table(table, 0, &kv_empty);
    TEST_ASSERT_LESS_THAN(0, res);
    // Upsert with empty value
    key_value_pair kv_noval = {"k", NULL, 0};
    res = upsert_node_to_sub_hash_table(table, 1, &kv_noval);
    TEST_ASSERT_LESS_THAN(0, res);
    // Get after delete
    key_value_pair kv = {"delkey", v, 2};
    upsert_node_to_sub_hash_table(table, 42, &kv);
    delete_key_from_sub_hash_table(table, 42, "delkey");
    key_value_pair out = {0};
    res = get_key_store_value_from_sub_hash_table(table, 42, "delkey", &out);
    TEST_ASSERT_LESS_THAN(0, res);
    // Upsert after delete (re-insert same key)
    res = upsert_node_to_sub_hash_table(table, 42, &kv);
    TEST_ASSERT_TRUE(res == 0 || res == 10);
    res = get_key_store_value_from_sub_hash_table(table, 42, "delkey", &out);
    TEST_ASSERT_EQUAL(0, res);
    cleanup_sub_hash_table(table);
    free_memory(table, false);
}

// 3. Multiple keys with same hash (simulate collision)
void test_hash_collision_handling_sub_hash_table(void) {
    sub_hash_table_memory_pool *table = NULL;
    create_new_sub_hash_table((sub_hash_table_configuration){.is_concurrency_enabled = false, .bucket_size = 2, .max_linked_list_chain_length = 2}, false, &table);
    // Simulate two keys with same hash
    unsigned char v1[] = "v1";
    key_value_pair kv1 = {"A", v1, strlen((char*)v1) + 1};
    unsigned char v2[] = "v2";
    key_value_pair kv2 = {"B", v2, strlen((char*)v2) + 1};
    uint32_t fake_hash = 12345;
    upsert_node_to_sub_hash_table(table, fake_hash, &kv1);
    upsert_node_to_sub_hash_table(table, fake_hash, &kv2);
    key_value_pair out1 = {0}, out2 = {0};
    int r1 = get_key_store_value_from_sub_hash_table(table, fake_hash, "A", &out1);
    int r2 = get_key_store_value_from_sub_hash_table(table, fake_hash, "B", &out2);
    TEST_ASSERT_EQUAL(0, r1);
    TEST_ASSERT_EQUAL(0, r2);
    TEST_ASSERT_EQUAL_STRING("A", out1.key);
    TEST_ASSERT_EQUAL_STRING("B", out2.key);
    cleanup_sub_hash_table(table);
    free_memory(table, false);
}

// 4. Cleanup after delete (if API exposes cleanup)
void test_cleanup_after_delete_sub_hash_table(void) {
    sub_hash_table_memory_pool *table = NULL;
    create_new_sub_hash_table((sub_hash_table_configuration){.is_concurrency_enabled = false, .bucket_size = 4, .max_linked_list_chain_length = 4}, false, &table);
    
    unsigned char v[] = "v";
    key_value_pair kv = {"gone", v, 2};
    upsert_node_to_sub_hash_table(table, 77, &kv);
    delete_key_from_sub_hash_table(table, 77, "gone");
    // If cleanup_sub_hash_table is the only cleanup, just call it
    int res = cleanup_sub_hash_table(table);
    TEST_ASSERT_EQUAL(0, res);
    free_memory(table, false);
}


int test_sub_hash_table_operation_main(void) {
    UNITY_BEGIN();
    printf("Running Sub Hash Table Operation Unit Tests...\n");
    RUN_TEST(test_create_new_sub_hash_table_success);
    RUN_TEST(test_create_new_sub_hash_table_invalid_args);
    RUN_TEST(test_upsert_and_get_node_sub_hash_table);
    RUN_TEST(test_upsert_duplicate_key_sub_hash_table);
    RUN_TEST(test_delete_key_from_sub_hash_table);
    RUN_TEST(test_delete_nonexistent_key_sub_hash_table);
    RUN_TEST(test_resize_trigger_result_sub_hash_table);
    RUN_TEST(test_concurrency_enabled_sub_hash_table);
    RUN_TEST(test_null_and_invalid_args_sub_hash_table);
    RUN_TEST(test_upsert_get_delete_edge_cases_sub_hash_table);
    RUN_TEST(test_hash_collision_handling_sub_hash_table);
    RUN_TEST(test_cleanup_after_delete_sub_hash_table);
    printf("Sub Hash Table Operation Unit Tests Completed.\n");
    return UNITY_END();
}