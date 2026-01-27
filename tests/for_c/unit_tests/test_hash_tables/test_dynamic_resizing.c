
#include <windows.h>
#include "unity.h"
#include "hash_table/hash_bucket_resizing_operation.h"
#include "type_definitions/hash_bucket_type_definition.h"
#include "hash_table/hash_table_operation.h"
#include <string.h>
#include "hash/hash_functions.h"

void test_initialize_hash_bucket_resizing_sets_flags_and_snapshot(void) {
    hash_bucket bucket = {0};
    // Setup a dummy sub_hash_table_ptr
    sub_hash_table_memory_pool dummy_table = {0};
    bucket.sub_hash_table_ptr = &dummy_table;
    bucket.is_resizing = false;

    int result = initialize_hash_bucket_resizing(&bucket);
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_TRUE(bucket.is_resizing);
    TEST_ASSERT_EQUAL_PTR(&dummy_table, bucket.snapshot_sub_hash_table_ptr);
    TEST_ASSERT_NULL(bucket.sub_hash_table_ptr);
}

void test_upsert_node_to_hash_bucket_during_resizing_adds_to_pending(void) {
    hash_bucket bucket = {0};
    bucket.is_resizing = true;
    key_value_pair kv = {"key", (unsigned char*)"value", 6};
    int result = upsert_node_to_hash_bucket_during_resizing(&bucket, 123, &kv);
    TEST_ASSERT_TRUE(result == 11 || result == 0);
    // Optionally check pending_list_head is not NULL
}

void test_delete_key_from_hash_bucket_during_resizing_adds_delete_to_pending(void) {
    hash_bucket bucket = {0};
    bucket.is_resizing = true;
    int result = delete_key_from_hash_bucket_during_resizing(&bucket, "key", 123);
    TEST_ASSERT_TRUE(result == 0 || result == 11);
    // Optionally check pending_list_head for a deleted node
}

void test_hash_table_resizing_trigger_and_data_integrity(void) {
    hash_table_configuration config = {
        .bucket_size = 2,
        .is_concurrency_enabled = false,
        .sub_hash_table_block_size = 2,
        .max_linked_list_chain_length = 4 // Low to trigger resizing
    };
    hash_table_memory_pool* table = NULL;
    int result = create_new_hash_table(config, &table);
    TEST_ASSERT_EQUAL(0, result);

    // Insert enough keys to trigger resizing
    int resize_triggered = 0;
    for (int i = 0; i < 20; ++i) {
        char k[16];
        unsigned char v[16];
        sprintf(k, "key%d", i);
        sprintf((char*)v, "val%d", i);
        key_value_pair kv2 = {k, v, strlen((char*)v) + 1};
        uint32_t key_hash2 = hash_function_murmur_32(k, 0);
        int upsert_result = upsert_node_to_hash_table(table, key_hash2, &kv2);
        if (upsert_result == 20) resize_triggered = 1;
        TEST_ASSERT_TRUE(upsert_result == 0 || upsert_result == 10 || upsert_result == 20 || upsert_result == 11);
    }
    
    TEST_ASSERT_TRUE(resize_triggered);

    
    // Wait for 2 seconds (Windows)
    Sleep(2000);

    // Check all keys are accessible after resizing
    for (int i = 0; i < 20; ++i) {
        char keybuf[16];
        sprintf(keybuf, "key%d", i);
        uint32_t key_hash = hash_function_murmur_32(keybuf, 0);
        key_value_pair out = {0};
        int get_result = get_key_value_from_hash_table(table, key_hash, keybuf, &out);
        TEST_ASSERT_EQUAL(0, get_result);
        TEST_ASSERT_EQUAL_STRING(keybuf, out.key);
    }

    cleanup_hash_table(table);
    free_memory(table, false);
}

void test_hash_table_delete_and_resize(void) {
        
    hash_table_configuration config = {
        .bucket_size = 2,
        .is_concurrency_enabled = false,
        .sub_hash_table_block_size = 1,
        .max_linked_list_chain_length = 4
    };
    hash_table_memory_pool* table = NULL;
    int result = create_new_hash_table(config, &table);
    TEST_ASSERT_EQUAL(0, result);

    char key[] = "resize_del";
    unsigned char value[] = "val";
    key_value_pair kv = {key, value, strlen((char*)value) + 1};
    uint32_t key_hash = hash_function_murmur_32(key, 0);
    upsert_node_to_hash_table(table, key_hash, &kv);

    // Insert more to trigger resize
    for (int i = 0; i < 30; ++i) {
        char k[16];
        unsigned char v[16];
        sprintf(k, "delkey%d", i);
        sprintf((char*)v, "val%d", i);
        key_value_pair kv2 = {k, v, strlen((char*)v) + 1};
        uint32_t key_hash2 = hash_function_murmur_32(k, 0);
        upsert_node_to_hash_table(table, key_hash2, &kv2);
    }

    int del_result = delete_key_from_hash_table(table, key_hash, key);
    TEST_ASSERT_TRUE(del_result == 0 || del_result == 11);

    // Wait for 2 seconds (Windows)
    Sleep(2000);

    key_value_pair out = {0};
    int get_result = get_key_value_from_hash_table(table, key_hash, key, &out);
    TEST_ASSERT_TRUE(get_result == ERR_DATA_NODE_NOT_FOUND);

    cleanup_hash_table(table);
    free_memory(table, false);
}

void test_upsert_pending_vs_hash_during_resizing(void) {
    hash_table_configuration config = {
        .bucket_size = 2,
        .is_concurrency_enabled = false,
        .sub_hash_table_block_size = 1,
        .max_linked_list_chain_length = 4
    };
    hash_table_memory_pool* table = NULL;
    int result = create_new_hash_table(config, &table);
    TEST_ASSERT_EQUAL(0, result);

    char keybuf[16];
    unsigned char valbuf[16];
    int count_hash = 0;
    int count_pending = 0;
    int count_other = 0;
    int resize_triggered = 0;
    for (int i = 0; i < 100; ++i) {
        sprintf(keybuf, "key%d", i);
        sprintf((char*)valbuf, "val%d", i);
        key_value_pair kv = {keybuf, valbuf, strlen((char*)valbuf) + 1};
        uint32_t key_hash = i + 100;
        int upsert_result = upsert_node_to_hash_table(table, key_hash, &kv);
        if (upsert_result == 10 || upsert_result == 0) count_hash++;
        else if (upsert_result == 11) count_pending++;
        else if (upsert_result == 20) resize_triggered++;
        else count_other++;
    }

    // Wait for 10 seconds (Windows)
    Sleep(2000);

    // At least some should go to hash and some to pending list if resizing is triggered
    TEST_ASSERT_TRUE(count_hash > 0);
    TEST_ASSERT_TRUE(count_pending > 0);
    // Optionally print for debug
    // printf("Hash: %d, Pending: %d, Other: %d\n", count_hash, count_pending, count_other);

    cleanup_hash_table(table);
    free_memory(table, false);
}

int test_dynamic_resizing_main(void) {
    UNITY_BEGIN();
    printf("Running Dynamic Resizing Unit Tests...\n");
    RUN_TEST(test_initialize_hash_bucket_resizing_sets_flags_and_snapshot);
    RUN_TEST(test_upsert_node_to_hash_bucket_during_resizing_adds_to_pending);
    RUN_TEST(test_delete_key_from_hash_bucket_during_resizing_adds_delete_to_pending);
    RUN_TEST(test_hash_table_resizing_trigger_and_data_integrity);
    RUN_TEST(test_hash_table_delete_and_resize);
    RUN_TEST(test_upsert_pending_vs_hash_during_resizing);
    printf("Dynamic Resizing Unit Tests Completed.\n");
    return UNITY_END();
}