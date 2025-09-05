#include "unity.h"
#include "bucket/hash_buckets.h"
#include "bucket/hash_bucket_list.h"
#include "core/data_node.h"
#include <stdlib.h>
#include <string.h>

void test_create_and_get_bucket(void) {
    hash_bucket buckets[2] = {0};
    hash_bucket *bucket = get_hash_bucket(buckets, 0, true);

    TEST_ASSERT_NOT_NULL_MESSAGE(bucket, "Bucket should not be NULL after creation.");
    TEST_ASSERT_EQUAL_MESSAGE(BUCKET_LIST, bucket->type, "Bucket type should be BUCKET_LIST after creation.");
    TEST_ASSERT_NULL_MESSAGE(bucket->container.list, "Bucket container.list should be NULL after creation.");
    TEST_ASSERT_EQUAL_MESSAGE(0, bucket->count, "Bucket count should be 0 after creation.");
}

void test_get_bucket_no_create(void) {
    hash_bucket buckets[2] = {0};
    hash_bucket *bucket = get_hash_bucket(buckets, 1, false);

    TEST_ASSERT_NULL_MESSAGE(bucket, "Bucket should be NULL when not created and accessed with create=false.");
}

void test_add_node_to_bucket_and_find(void) {
    const char *key = "key1";
    uint32_t key_hash = 54323;
    const char *data = "data1";
    size_t data_size = strlen(data) + 1;
    hash_bucket buckets[1] = {0};
    hash_bucket *bucket = get_hash_bucket(buckets, 0, true);
    data_node *node = create_data_node(key, key_hash, (const unsigned char *)data, data_size);

    int res = add_node_to_bucket(bucket, key_hash, node);
    data_node *found = find_node_in_bucket(bucket, key, key_hash);

    TEST_ASSERT_EQUAL_MESSAGE(0, res, "add_node_to_bucket should return 0 on success.");
    TEST_ASSERT_NOT_NULL_MESSAGE(found, "find_node_in_bucket should find the node just added.");
    TEST_ASSERT_EQUAL_STRING_MESSAGE(data, found->data, "Data in found node should match original data.");

    delete_data_node(node);
}

void test_find_node_in_bucket_invalid(void) {
    data_node *found = find_node_in_bucket(NULL, "key", 123);
    TEST_ASSERT_NULL_MESSAGE(found, "find_node_in_bucket should return NULL when bucket is NULL.");
}

void test_delete_node_from_bucket(void) {
    const char *key = "key2";
    uint32_t key_hash = 54321;
    const unsigned char data[] = "data2";
    size_t data_size = sizeof(data);
    
    hash_bucket buckets[1] = {0};
    hash_bucket *bucket = get_hash_bucket(buckets, 0, true);
    TEST_ASSERT_NOT_NULL_MESSAGE(bucket, "Bucket should not be NULL after creation.");
    data_node *node = create_data_node(key, key_hash, data, data_size);
    int result = add_node_to_bucket(bucket, key_hash, node);
    TEST_ASSERT_EQUAL_MESSAGE(0, result, "add_node_to_bucket should return 0 on success.");

    result = delete_node_from_bucket(bucket, key, key_hash);
    TEST_ASSERT_EQUAL_MESSAGE(0, result, "delete_node_from_bucket should return 0 on successful deletion.");
    
    data_node *found = find_node_in_bucket(bucket, key, key_hash);
    TEST_ASSERT_NULL_MESSAGE(found, "find_node_in_bucket should return NULL after node is deleted.");
}

void test_delete_node_from_bucket_invalid(void) {
    int res = delete_node_from_bucket(NULL, "key", 123);

    TEST_ASSERT_EQUAL_MESSAGE(-1, res, "delete_node_from_bucket should return -1 when bucket is NULL.");
}

void test_check_if_bucket_container_exists(void) {
    hash_bucket bucket = {0};
    bucket.type = BUCKET_LIST;
    bucket.container.list = NULL;

    TEST_ASSERT_FALSE_MESSAGE(check_if_bucket_container_exists(&bucket), "check_if_bucket_container_exists should return false when container.list is NULL.");

    list_node dummy = {0};
    bucket.container.list = &dummy;

    TEST_ASSERT_TRUE_MESSAGE(check_if_bucket_container_exists(&bucket), "check_if_bucket_container_exists should return true when container.list is not NULL.");
}

void test_create_bucket(void) {
    hash_bucket buckets[2] = {0};
    hash_bucket *bucket = get_hash_bucket(buckets, 1, true);

    TEST_ASSERT_NOT_NULL_MESSAGE(bucket, "Bucket should not be NULL after creation.");
    TEST_ASSERT_EQUAL_MESSAGE(BUCKET_LIST, bucket->type, "Bucket type should be BUCKET_LIST after creation.");
    TEST_ASSERT_NULL_MESSAGE(bucket->container.list, "Bucket container.list should be NULL after creation.");
    TEST_ASSERT_EQUAL_MESSAGE(0, bucket->count, "Bucket count should be 0 after creation.");
}

void test_unsupported_bucket_type(void) {
    hash_bucket bucket = {0};
    bucket.type = 999; // Invalid type
    data_node node = {0};

    int res = add_node_to_bucket(&bucket, 1, &node);
    data_node *found = find_node_in_bucket(&bucket, "key", 1);
    int del_res = delete_node_from_bucket(&bucket, "key", 1);

    TEST_ASSERT_EQUAL_MESSAGE(-1, res, "add_node_to_bucket should return -1 for unsupported bucket type.");
    TEST_ASSERT_NULL_MESSAGE(found, "find_node_in_bucket should return NULL for unsupported bucket type.");
    TEST_ASSERT_EQUAL_MESSAGE(-1, del_res, "delete_node_from_bucket should return -1 for unsupported bucket type.");
}

void test_delete_hash_bucket(void) {
    hash_bucket *bucket = malloc(sizeof(hash_bucket));
    bucket->type = BUCKET_LIST;
    bucket->container.list = NULL;
    bucket->count = 0;

    delete_hash_bucket(bucket); // Should not crash

    // No assertion needed, just ensure no crash
    free(bucket);
}

void test_multiple_nodes_collision(void) {
    hash_bucket buckets[1] = {0};
    hash_bucket *bucket = get_hash_bucket(buckets, 0, true);
    TEST_ASSERT_NOT_NULL_MESSAGE(bucket, "Bucket should not be NULL after creation.");
    uint32_t hash = 12345;
    data_node *node1 = create_data_node("keyA", hash, (unsigned char*)"dataA", 6);
    TEST_ASSERT_NOT_NULL_MESSAGE(node1, "create_data_node should succeed for node1.");
    data_node *node2 = create_data_node("keyB", hash, (unsigned char*)"dataB", 6);
    TEST_ASSERT_NOT_NULL_MESSAGE(node2, "create_data_node should succeed for node2.");

    int result = add_node_to_bucket(bucket, hash, node1);
    TEST_ASSERT_EQUAL_MESSAGE(0, result, "add_node_to_bucket should return 0 on success for node1.");
    result = add_node_to_bucket(bucket, hash, node2);
    TEST_ASSERT_EQUAL_MESSAGE(0, result, "add_node_to_bucket should return 0 on success for node2.");

    data_node *foundA = find_node_in_bucket(bucket, "keyA", hash);
    data_node *foundB = find_node_in_bucket(bucket, "keyB", hash);
    TEST_ASSERT_NOT_NULL_MESSAGE(foundA, "find_node_in_bucket should find node1 with colliding hash.");
    TEST_ASSERT_NOT_NULL_MESSAGE(foundB, "find_node_in_bucket should find node2 with colliding hash.");
    
    delete_node_from_bucket(bucket, "keyA", hash);
    delete_node_from_bucket(bucket, "keyB", hash);
}

void test_empty_key_and_data(void) {
    hash_bucket buckets[1] = {0};
    hash_bucket *bucket = get_hash_bucket(buckets, 0, true);
    data_node *node = create_data_node("", 1, (unsigned char*)"", 1);
    int result = add_node_to_bucket(bucket, 1, node);
    TEST_ASSERT_EQUAL_MESSAGE(-1, result, "add_node_to_bucket should return -1 on failure for empty key and data.");
    data_node *found = find_node_in_bucket(bucket, "", 1);
    TEST_ASSERT_NULL_MESSAGE(found, "find_node_in_bucket should return NULL for empty key and data.");
    delete_node_from_bucket(bucket, "", 1);
}

void test_long_key_and_data(void) {
    char long_key[1024];
    char long_data[2048];
    memset(long_key, 'K', sizeof(long_key)-1); long_key[1023] = '\0';
    memset(long_data, 'D', sizeof(long_data)-1); long_data[2047] = '\0';
    hash_bucket buckets[1] = {0};
    hash_bucket *bucket = get_hash_bucket(buckets, 0, true);
    data_node *node = create_data_node(long_key, 99999, (unsigned char*)long_data, sizeof(long_data));
    add_node_to_bucket(bucket, 99999, node);
    data_node *found = find_node_in_bucket(bucket, long_key, 99999);
    TEST_ASSERT_NOT_NULL_MESSAGE(found, "find_node_in_bucket should find node with long key and data.");
    delete_node_from_bucket(bucket, long_key, 99999);
}

void test_null_key_and_data(void) {
    hash_bucket buckets[1] = {0};
    hash_bucket *bucket = get_hash_bucket(buckets, 0, true);
    data_node *node = create_data_node(NULL, 1, NULL, 0);
    int res = add_node_to_bucket(bucket, 1, node);
    TEST_ASSERT_EQUAL_MESSAGE(-1, res, "add_node_to_bucket should return -1 even if key/data is NULL.");
    data_node *found = find_node_in_bucket(bucket, NULL, 1);
    TEST_ASSERT_NULL_MESSAGE(found, "find_node_in_bucket should return NULL when searching for NULL key.");
    delete_node_from_bucket(bucket, NULL, 1);
}

void test_double_deletion(void) {
    hash_bucket buckets[1] = {0};
    hash_bucket *bucket = get_hash_bucket(buckets, 0, true);
    data_node *node = create_data_node("key", 2, (unsigned char*)"data", 5);
    add_node_to_bucket(bucket, 2, node);
    delete_node_from_bucket(bucket, "key", 2);
    int res = delete_node_from_bucket(bucket, "key", 2);
    TEST_ASSERT_EQUAL_MESSAGE(-1, res, "delete_node_from_bucket should return -1 when deleting already deleted node.");
}

void test_repeated_add_find_delete(void) {
    hash_bucket buckets[1] = {0};
    hash_bucket *bucket = get_hash_bucket(buckets, 0, true);
    for (int i = 0; i < 10; ++i) {
        char key[16];
        snprintf(key, sizeof(key), "key%d", i);
        data_node *node = create_data_node(key, i, (unsigned char*)"data", 5);
        add_node_to_bucket(bucket, i, node);
        data_node *found = find_node_in_bucket(bucket, key, i);
        TEST_ASSERT_NOT_NULL_MESSAGE(found, "find_node_in_bucket should find node with key and hash.");
        delete_node_from_bucket(bucket, key, i);
    }
}

void test_bucket_tree_type(void) {
    hash_bucket bucket = {0};
    bucket.type = BUCKET_TREE;
    bucket.container.tree = NULL;
    data_node node = {0};
    int res = add_node_to_bucket(&bucket, 1, &node);
    TEST_ASSERT_EQUAL_MESSAGE(-1, res, "add_node_to_bucket should return -1 for BUCKET_TREE type (unsupported).");
    data_node *found = find_node_in_bucket(&bucket, "key", 1);
    TEST_ASSERT_NULL_MESSAGE(found, "find_node_in_bucket should return NULL for BUCKET_TREE type (unsupported).");
    int del_res = delete_node_from_bucket(&bucket, "key", 1);
    TEST_ASSERT_EQUAL_MESSAGE(-1, del_res, "delete_node_from_bucket should return -1 for BUCKET_TREE type (unsupported).");
}

void test_invalid_index(void) {
    hash_bucket buckets[1] = {0};
    hash_bucket *bucket_neg = get_hash_bucket(buckets, -1, true);
    TEST_ASSERT_NULL_MESSAGE(bucket_neg, "get_hash_bucket should return NULL for negative index.");
}

void test_corrupted_bucket_ptr(void) {
    hash_bucket *bucket = NULL;
    hash_bucket *result = get_hash_bucket(bucket, 0, true);
    TEST_ASSERT_NULL_MESSAGE(result, "get_hash_bucket should return NULL when buckets pointer is NULL.");
}

void test_memory_allocation_failure(void) {
    data_node *node = create_data_node(NULL, 1, NULL, 0);
    TEST_ASSERT_NULL_MESSAGE(node, "create_data_node should return NULL when key/data is NULL and size is 0.");
    delete_data_node(node);
}

int test_hash_buckets_suite(void) {
    RUN_TEST(test_create_and_get_bucket);
    RUN_TEST(test_get_bucket_no_create);
    RUN_TEST(test_add_node_to_bucket_and_find);
    RUN_TEST(test_find_node_in_bucket_invalid);
    RUN_TEST(test_delete_node_from_bucket);
    RUN_TEST(test_delete_node_from_bucket_invalid);
    RUN_TEST(test_check_if_bucket_container_exists);
    RUN_TEST(test_create_bucket);
    RUN_TEST(test_unsupported_bucket_type);
    RUN_TEST(test_delete_hash_bucket);
    RUN_TEST(test_multiple_nodes_collision);
    RUN_TEST(test_empty_key_and_data);
    RUN_TEST(test_long_key_and_data);
    RUN_TEST(test_null_key_and_data);
    RUN_TEST(test_double_deletion);
    RUN_TEST(test_repeated_add_find_delete);
    RUN_TEST(test_bucket_tree_type);
    RUN_TEST(test_invalid_index);
    RUN_TEST(test_corrupted_bucket_ptr);
    RUN_TEST(test_memory_allocation_failure);
    return 0;
}
