#include "unity.h"
#include "bucket/hash_buckets.h"
#include "bucket/hash_bucket_list.h"
#include "core/data_node.h"
#include "utils/memory_manager.h"
#include <stdlib.h>
#include <string.h>

static data_node* make_test_data_node(const char *key, uint32_t key_hash, const unsigned char *data, size_t data_size) {
    size_t key_len = strlen(key) + 1;
    data_node *node = (data_node *)malloc(sizeof(data_node) + key_len);
    if (node == NULL) {
        return NULL; // Return NULL on allocation failure
    }
    memcpy(node->key, key, key_len);
    node->key_hash = key_hash;
    node->data_size = data_size;
    node->data = (unsigned char *)malloc(data_size);
    if (node->data == NULL) {
        free(node);
        return NULL;
    }
    memcpy(node->data, data, data_size);
    return node;
}

void test_initialise_and_cleanup_hash_buckets(void) {
    TEST_ASSERT_EQUAL(-1, initialise_hash_buckets(0, false)); // Not power of two
    TEST_ASSERT_EQUAL(0, initialise_hash_buckets(8, false));
    TEST_ASSERT_EQUAL(0, cleanup_hash_buckets());
}

void test_get_hash_bucket_and_initialization(void) {
    initialise_hash_buckets(4, false);
    for (unsigned int i = 0; i < 4; ++i) {
        hash_bucket *bucket = get_hash_bucket(i, true);
        TEST_ASSERT_NOT_NULL(bucket);
        TEST_ASSERT_TRUE(bucket->is_initialized);
    }
    cleanup_hash_buckets();
}

void test_get_hash_bucket_out_of_bounds(void) {
    initialise_hash_buckets(2, false);
    hash_bucket *bucket = get_hash_bucket(2, false); // Out of bounds
    TEST_ASSERT_NULL(bucket);
    cleanup_hash_buckets();
}

void test_add_and_find_node_in_bucket(void) {
    initialise_hash_buckets(2, false);
    data_node* dummy_node = make_test_data_node("key1", 123, (unsigned char *)"data", 5);
    TEST_ASSERT_NOT_NULL_MESSAGE(dummy_node, "Failed to create test data node");
    TEST_ASSERT_EQUAL_MESSAGE(0, add_node_to_bucket(0, 123, dummy_node), "Failed to add node to bucket");
    printf("Added node with key: %s\n", dummy_node->key);
    data_node *found = find_node_in_bucket(0, "key1", 123);
    TEST_ASSERT_NOT_NULL_MESSAGE(found, "Failed to find node in bucket");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("key1", found->key, "Key mismatch");
    cleanup_hash_buckets();
}

void test_add_node_to_invalid_bucket(void) {
    initialise_hash_buckets(2, false);
    data_node* dummy_node = make_test_data_node("key2", 456, (unsigned char *)"data2", 6);
    TEST_ASSERT_EQUAL(-1, add_node_to_bucket(5, 456, dummy_node)); // Out of bounds
    cleanup_hash_buckets();
}

void test_delete_node_from_bucket(void) {
    initialise_hash_buckets(2, false);
    data_node* dummy_node = make_test_data_node("key3", 789, (unsigned char *)"data3", 6);
    add_node_to_bucket(1, 789, dummy_node);
    TEST_ASSERT_EQUAL(0, delete_node_from_bucket(1, "key3", 789));
    TEST_ASSERT_NULL(find_node_in_bucket(1, "key3", 789));
    cleanup_hash_buckets();
}

void test_delete_node_from_invalid_bucket(void) {
    initialise_hash_buckets(2, false);
    TEST_ASSERT_EQUAL(-1, delete_node_from_bucket(3, "keyX", 999)); // Out of bounds
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
    // Add node with NULL pointer
    TEST_ASSERT_EQUAL(-1, add_node_to_bucket(1, 123, NULL));
    cleanup_hash_buckets();
}

void test_find_node_null_key(void) {
    initialise_hash_buckets(2, false);
    // Find node with NULL key
    TEST_ASSERT_NULL(find_node_in_bucket(1, NULL, 123));
    cleanup_hash_buckets();
}

void test_delete_node_null_key(void) {
    initialise_hash_buckets(2, false);
    // Delete node with NULL key
    TEST_ASSERT_EQUAL(-1, delete_node_from_bucket(1, NULL, 123));
    cleanup_hash_buckets();
}

void test_add_node_after_cleanup(void) {
    initialise_hash_buckets(2, false);
    cleanup_hash_buckets();
    // Try to add node after cleanup
    data_node* dummy_node = make_test_data_node("keyX", 321, (unsigned char *)"dataX", 6);
    TEST_ASSERT_EQUAL(-1, add_node_to_bucket(1, 321, dummy_node));
    if (dummy_node) {
        free(dummy_node->data);
        free(dummy_node);
    }
}

int test_hash_buckets_suite(void) {
    
    printf("Running hash_buckets tests...\n");
    initialize_memory_manager((memory_manager_config){ .bucket_size = 10, .pre_allocation_factor = 1.0, .allocate_list_pool = true, .allocate_tree_pool = false });
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


