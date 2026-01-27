#include "unity.h"
#include <string.h>
#include <stdlib.h>
#include "hash_table/hash_table_operation.h"
#include "utils/memory_manager.h"

void test_create_new_hash_table_should_initialize_and_return_success(void) {
    hash_table_configuration config = { .bucket_size = 4, .is_concurrency_enabled = false, .sub_hash_table_block_size = 2, .max_linked_list_chain_length = 3 };
    hash_table_memory_pool* table = NULL;
    int result = create_new_hash_table(config, &table);
    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_NOT_NULL(table);
    cleanup_hash_table(table);
    free_memory(table, false);
}

void test_cleanup_hash_table_should_return_success_on_valid_table(void) {
    hash_table_configuration config = { .bucket_size = 2, .is_concurrency_enabled = false, .sub_hash_table_block_size = 1, .max_linked_list_chain_length = 2 };
    hash_table_memory_pool* table = NULL;
    create_new_hash_table(config, &table);
    int result = cleanup_hash_table(table);
    TEST_ASSERT_EQUAL_INT(0, result);
    free_memory(table, false);
}

void test_upsert_and_get_key_value_from_hash_table(void) {
    hash_table_configuration config = { .bucket_size = 2, .is_concurrency_enabled = false, .sub_hash_table_block_size = 1, .max_linked_list_chain_length = 2 };
    hash_table_memory_pool* table = NULL;
    create_new_hash_table(config, &table);
    const char* key = "testkey";
    unsigned char value[] = {1,2,3};
    key_value_pair kv = { .key = (char*)key, .value = value, .value_size = sizeof(value) };
    uint32_t key_hash = 0x12345678;
    int upsert_result = upsert_node_to_hash_table(table, key_hash, &kv);
    TEST_ASSERT_TRUE(upsert_result == 0 || upsert_result == 10);
    key_value_pair out = {0};
    int get_result = get_key_value_from_hash_table(table, key_hash, key, &out);
    TEST_ASSERT_EQUAL_INT(0, get_result);
    TEST_ASSERT_EQUAL_STRING(key, out.key);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(value, out.value, out.value_size);
    cleanup_hash_table(table);
    free_memory(table, false);
}

void test_delete_key_from_hash_table_should_remove_key(void) {
    hash_table_configuration config = { .bucket_size = 2, .is_concurrency_enabled = false, .sub_hash_table_block_size = 1, .max_linked_list_chain_length = 2 };
    hash_table_memory_pool* table = NULL;
    create_new_hash_table(config, &table);
    const char* key = "delkey";
    unsigned char value[] = {9,8,7};
    key_value_pair kv = { .key = (char*)key, .value = value, .value_size = sizeof(value) };
    uint32_t key_hash = 0x87654321;
    int result = upsert_node_to_hash_table(table, key_hash, &kv);
    TEST_ASSERT_TRUE(result == 0 || result == 10);
    int del_result = delete_key_from_hash_table(table, key_hash, key);
    TEST_ASSERT_EQUAL_INT(0, del_result);
    key_value_pair out = {0};
    int get_result = get_key_value_from_hash_table(table, key_hash, key, &out);
    TEST_ASSERT_LESS_THAN_INT(0, get_result); // Should not find
    cleanup_hash_table(table);
    free_memory(table, false);
}

void test_get_key_value_from_hash_table_should_fail_for_missing_key(void) {
    hash_table_configuration config = { .bucket_size = 2, .is_concurrency_enabled = false, .sub_hash_table_block_size = 1, .max_linked_list_chain_length = 2 };
    hash_table_memory_pool* table = NULL;
    create_new_hash_table(config, &table);
    key_value_pair out = {0};
    int get_result = get_key_value_from_hash_table(table, 0x11111111, "notfound", &out);
    TEST_ASSERT_LESS_THAN_INT(0, get_result);
    cleanup_hash_table(table);
    free_memory(table, false);
}


int test_hash_table_operation_main(void) {
    UNITY_BEGIN();
    printf("Running Hash Table Operation Unit Tests...\n");
    RUN_TEST(test_create_new_hash_table_should_initialize_and_return_success);
    RUN_TEST(test_cleanup_hash_table_should_return_success_on_valid_table);
    RUN_TEST(test_upsert_and_get_key_value_from_hash_table);
    RUN_TEST(test_delete_key_from_hash_table_should_remove_key);
    RUN_TEST(test_get_key_value_from_hash_table_should_fail_for_missing_key);
    printf("Hash Table Operation Unit Tests Completed.\n");
    return UNITY_END();
}
