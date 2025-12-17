#include "unity.h"
#include "bucket/hash_buckets.h"
#include "bucket/hash_bucket_list.h"
#include "core/data_node.h"
#include "utils/memory_manager.h"
#include <stdlib.h>
#include <string.h>



void test_initialise_and_cleanup_hash_buckets(void) {
    TEST_ASSERT_EQUAL(-21, initialise_hash_buckets(0, false)); // Not power of two
    TEST_ASSERT_EQUAL(0, initialise_hash_buckets(8, false));
    TEST_ASSERT_EQUAL(0, cleanup_hash_buckets());
}

void test_get_hash_bucket_and_initialization(void) {
    initialise_hash_buckets(4, false);
    for (unsigned int i = 0; i < 4; ++i) {
        hash_bucket *bucket = get_hash_bucket(i);
        TEST_ASSERT_NOT_NULL(bucket);
        TEST_ASSERT_TRUE(bucket->is_initialized);
    }
    cleanup_hash_buckets();
}

void test_get_hash_bucket_out_of_bounds(void) {
    initialise_hash_buckets(2, false);
    hash_bucket *bucket = get_hash_bucket(2); // Out of bounds
    TEST_ASSERT_NULL(bucket);
    cleanup_hash_buckets();
}

void test_add_and_find_node_in_bucket(void) {
    initialise_hash_buckets(2, false);
    unsigned char data[] = "data";
    key_store_value value = { .data = data, .data_size = sizeof(data) };
    TEST_ASSERT_EQUAL_MESSAGE(0, add_node_to_bucket(0, "key1", 123, &value), "Failed to add node to bucket");
    key_store_value out = {0};
    TEST_ASSERT_EQUAL_MESSAGE(0, find_node_in_bucket(0, "key1", 123, &out), "Failed to find node in bucket");
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(data, out.data, sizeof(data), "Data mismatch");
    free(out.data);
    cleanup_hash_buckets();
}

void test_add_node_to_invalid_bucket(void) {
    initialise_hash_buckets(2, false);
    unsigned char data[] = "data2";
    key_store_value value = { .data = data, .data_size = sizeof(data) };
    TEST_ASSERT_EQUAL(-40, add_node_to_bucket(5, "key2", 456, &value)); // Out of bounds
    cleanup_hash_buckets();
}

void test_delete_node_from_bucket(void) {
    initialise_hash_buckets(2, false);
    unsigned char data[] = "data3";
    key_store_value value = { .data = data, .data_size = sizeof(data) };
    add_node_to_bucket(1, "key3", 789, &value);
    TEST_ASSERT_EQUAL(0, delete_node_from_bucket(1, "key3", 789));
    key_store_value out = {0};
    TEST_ASSERT_EQUAL(-41, find_node_in_bucket(1, "key3", 789, &out));
    cleanup_hash_buckets();
}

void test_delete_node_from_invalid_bucket(void) {
    initialise_hash_buckets(2, false);
    TEST_ASSERT_EQUAL(-40, delete_node_from_bucket(3, "keyX", 999)); // Out of bounds
    cleanup_hash_buckets();
}

void test_repeated_initialise_and_cleanup(void) {
    // Repeated initialisation and cleanup
    TEST_ASSERT_EQUAL(0, initialise_hash_buckets(4, false));
    TEST_ASSERT_EQUAL(0, cleanup_hash_buckets());
    // Cleanup again should fail
    TEST_ASSERT_EQUAL(0, cleanup_hash_buckets());
    // Re-initialise after cleanup
    TEST_ASSERT_EQUAL(0, initialise_hash_buckets(8, false));
    TEST_ASSERT_EQUAL(0, cleanup_hash_buckets());
}

void test_add_null_node(void) {
    initialise_hash_buckets(2, false);
    // Add node with NULL value
    TEST_ASSERT_EQUAL(-20, add_node_to_bucket(1, "key", 123, NULL));
    cleanup_hash_buckets();
}

void test_find_node_null_key(void) {
    initialise_hash_buckets(2, false);
    key_store_value out = {0};
    // Find node with NULL key
    TEST_ASSERT_EQUAL(-20, find_node_in_bucket(1, NULL, 123, &out));
    cleanup_hash_buckets();
}

void test_delete_node_null_key(void) {
    initialise_hash_buckets(2, false);
    // Delete node with NULL key
    TEST_ASSERT_EQUAL(-20, delete_node_from_bucket(1, NULL, 123));
    cleanup_hash_buckets();
}

void test_add_node_after_cleanup(void) {
    initialise_hash_buckets(2, false);
    cleanup_hash_buckets();
    // Try to add node after cleanup
    unsigned char data[] = "dataX";
    key_store_value value = { .data = data, .data_size = sizeof(data) };
    TEST_ASSERT_EQUAL(-40, add_node_to_bucket(1, "keyX", 321, &value));
}

int test_hash_buckets_suite(void) {
    
    printf("Running hash_buckets tests...\n");
    initialize_memory_manager((memory_manager_config){ .bucket_size = 10, .pre_allocation_factor = 1.0, .allocate_list_pool = true, .allocate_tree_pool = false, .is_concurrency_enabled = false });
    RUN_TEST(test_initialise_and_cleanup_hash_buckets);
    RUN_TEST(test_get_hash_bucket_and_initialization);
    RUN_TEST(test_get_hash_bucket_out_of_bounds);
    RUN_TEST(test_add_and_find_node_in_bucket);
    RUN_TEST(test_add_node_to_invalid_bucket);
    RUN_TEST(test_delete_node_from_bucket);
    RUN_TEST(test_delete_node_from_invalid_bucket);
    RUN_TEST(test_repeated_initialise_and_cleanup);
    RUN_TEST(test_add_null_node);
    RUN_TEST(test_find_node_null_key);
    RUN_TEST(test_delete_node_null_key);
    RUN_TEST(test_add_node_after_cleanup);
    cleanup_memory_manager();
    printf("hash_buckets tests completed.\n");
    return 0;
}


