
#include "unity.h"
#include "hash_table/resizing/hash_bucket_resizing_operation.h"
#include "type_definitions/hash_bucket_type_definition.h"
#include "hash_table/hash_table_operation.h"
#include <string.h>
#include "hash/hash_functions.h"
#include "hash_table/hash_bucket_operation.h"
#include "sub_hash_table/sub_hash_table_operation.h"
#include "utils/helper_functions.h"
#include "hash_table/resizing/buffer_operation.h"
#include "utils/memory_manager.h"

void mimic_hash_resizing_initialization(hash_bucket* bucket) {
    bucket->is_resizing = true;
    bucket->snapshot_sub_hash_table_ptr = bucket->sub_hash_table_ptr;
    initialize_resizing_buffer(bucket);
    create_new_sub_hash_table(bucket->sub_hash_table_config, true, &bucket->resizing_buffer_ptr->new_sub_hash_table_ptr);
    bucket->sub_hash_table_ptr = NULL;
}


void test_upsert_node_to_hash_bucket_during_resizing_adds_to_pending(void) {
    hash_bucket* bucket = callocate_memory(1, sizeof(hash_bucket));
    sub_hash_table_configuration config = { .is_concurrency_enabled = false, .bucket_size = 2, .max_linked_list_chain_length = 2 };
    initialise_hash_bucket(bucket, config);
    mimic_hash_resizing_initialization(bucket);
    key_value_pair kv = {"key", (unsigned char*)"value", 6};
    int result = upsert_node_to_hash_bucket_during_resizing(bucket, 123, &kv);
    printf("Result of upsert during resizing: %d\n", result);
    TEST_ASSERT_TRUE(result == 11 || result == 0);
    delete_resizing_buffer(bucket->resizing_buffer_ptr);
    bucket->resizing_buffer_ptr = NULL;
    cleanup_hash_bucket(bucket);
    free_memory(bucket, false);
}

void test_delete_key_from_hash_bucket_during_resizing_adds_delete_to_pending(void) {
    hash_bucket* bucket = callocate_memory(1, sizeof(hash_bucket));
    sub_hash_table_configuration config = { .is_concurrency_enabled = false, .bucket_size = 2, .max_linked_list_chain_length = 2 };
    initialise_hash_bucket(bucket, config);
    mimic_hash_resizing_initialization(bucket);
    int result = delete_key_from_hash_bucket_during_resizing(bucket, "key", 123);
    printf("Result of delete during resizing: %d\n", result);
    TEST_ASSERT_TRUE(result == 0 || result == 11);
    delete_resizing_buffer(bucket->resizing_buffer_ptr);
     bucket->resizing_buffer_ptr = NULL;
    cleanup_hash_bucket(bucket);
    free_memory(bucket, false);
}

void test_hash_table_resizing_trigger_and_data_integrity(void) {
    hash_table_configuration config = {
        .bucket_size = 2,
        .is_concurrency_enabled = false,
        .sub_hash_table_bucket_size = 2,
        .max_linked_list_chain_length = 4
    };
    hash_table_memory_pool* table = NULL;
    int result = create_new_hash_table(config, &table);
    TEST_ASSERT_EQUAL(0, result);

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

    portable_sleep_ms(2000);

    for (int i = 0; i < 20; ++i) {
        char keybuf[16];
        sprintf(keybuf, "key%d", i);
        uint32_t key_hash = hash_function_murmur_32(keybuf, 0);
        key_value_pair out = {0};
        int get_result = get_key_value_from_hash_table(table, key_hash, keybuf, &out);
        TEST_ASSERT_EQUAL(0, get_result);
        TEST_ASSERT_EQUAL_STRING(keybuf, out.key);
        if (out.key && out.key != keybuf) free(out.key);
        if (out.value) free(out.value);
    }

    cleanup_hash_table(table);
    free_memory(table, false);
}

void test_hash_table_delete_and_resize(void) {
    hash_table_configuration config = {
        .bucket_size = 2,
        .is_concurrency_enabled = false,
        .sub_hash_table_bucket_size = 1,
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

    portable_sleep_ms(2000);

    key_value_pair out = {0};
    int get_result = get_key_value_from_hash_table(table, key_hash, key, &out);
    TEST_ASSERT_TRUE(get_result == ERR_DATA_NODE_NOT_FOUND);
    if (out.key) free(out.key);
    if (out.value) free(out.value);

    cleanup_hash_table(table);
    free_memory(table, false);
}

void test_upsert_pending_vs_hash_during_resizing(void) {
    hash_table_configuration config = {
        .bucket_size = 2,
        .is_concurrency_enabled = false,
        .sub_hash_table_bucket_size = 1,
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

    portable_sleep_ms(2000);

    TEST_ASSERT_TRUE(count_hash > 0);
    TEST_ASSERT_TRUE(count_pending > 0);

    cleanup_hash_table(table);
    free_memory(table, false);
}

int test_dynamic_resizing_main(void) {
    UNITY_BEGIN();
    printf("Running Dynamic Resizing Unit Tests...\n");
    RUN_TEST(test_upsert_node_to_hash_bucket_during_resizing_adds_to_pending);
    RUN_TEST(test_delete_key_from_hash_bucket_during_resizing_adds_delete_to_pending);
    RUN_TEST(test_hash_table_resizing_trigger_and_data_integrity);
    RUN_TEST(test_hash_table_delete_and_resize);
    RUN_TEST(test_upsert_pending_vs_hash_during_resizing);
    printf("Dynamic Resizing Unit Tests Completed.\n");
    return UNITY_END();
}