#include "unity.h"
#include "bucket/hash_bucket_list.h"
#include "core/data_node.h"
#include "utils/memory_manager.h"
#include <string.h>

void test_insert_and_find_list_node(void) {
    const char *key = "testkey";
    uint32_t key_hash = 12345;
    unsigned char data[] = "value";
    size_t data_size = sizeof(data);

    data_node *dnode = create_data_node(key, key_hash, data, data_size);
    TEST_ASSERT_NOT_NULL(dnode);

    list_node *head = NULL;
    int result = insert_list_node(&head, key_hash, dnode);
    TEST_ASSERT_EQUAL(0, result);

    list_node *found = find_list_node(head, key, key_hash);
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_EQUAL_PTR(dnode, found->data);

    delete_list_node(&head, key, key_hash);
}

void test_delete_list_node_not_found(void) {
    list_node *head = NULL;
    int result = delete_list_node(&head, "notfound", 99999);
    TEST_ASSERT_EQUAL(-1, result);
}

void test_find_list_node_not_found(void) {
    list_node *head = NULL;
    list_node *found = find_list_node(head, "notfound", 99999);
    TEST_ASSERT_NULL(found);
}

void test_insert_multiple_nodes_and_find(void) {
    list_node *head = NULL;
    const char *keys[] = {"key1", "key2", "key3"};
    uint32_t hashes[] = {111, 222, 333};
    unsigned char data[] = "value";
    size_t data_size = sizeof(data);

    data_node *nodes[3];
    for (int i = 0; i < 3; ++i) {
        nodes[i] = create_data_node(keys[i], hashes[i], data, data_size);
        TEST_ASSERT_NOT_NULL_MESSAGE(nodes[i], "Failed to create data node");
        int result = insert_list_node(&head, hashes[i], nodes[i]);
        TEST_ASSERT_EQUAL(0, result);
    }

    for (int i = 0; i < 3; ++i) {
        char msg[64];
        snprintf(msg, sizeof(msg), "find_list_node should find node with key '%s' and hash %d.", keys[i], hashes[i]);
        list_node *found = find_list_node(head, keys[i], hashes[i]);
        TEST_ASSERT_NOT_NULL_MESSAGE(found, msg);
        TEST_ASSERT_EQUAL_PTR(nodes[i], found->data);
    }

    // Cleanup
    for (int i = 0; i < 3; ++i) {
        char msg[64];
        snprintf(msg, sizeof(msg), "delete_list_node should delete node with key '%s' and hash %d.", keys[i], hashes[i]);
        int result = delete_list_node(&head, keys[i], hashes[i]);
        TEST_ASSERT_EQUAL(0, result);
    }
}

void test_delete_head_and_middle_node(void) {
    list_node *head = NULL;
    const char *key1 = "head";
    const char *key2 = "middle";
    uint32_t hash1 = 1, hash2 = 2;
    unsigned char data[] = "value";
    size_t data_size = sizeof(data);

    data_node *node1 = create_data_node(key1, hash1, data, data_size);
    data_node *node2 = create_data_node(key2, hash2, data, data_size);

    int result = insert_list_node(&head, hash2, node2); // middle
    result = insert_list_node(&head, hash1, node1); // head

    // Delete head
    result = delete_list_node(&head, key1, hash1);
    TEST_ASSERT_EQUAL(0, result);

    // Delete middle
    result = delete_list_node(&head, key2, hash2);
    TEST_ASSERT_EQUAL(0, result);
}

void test_insert_null_data(void) {
    list_node *head = NULL;
    int result = insert_list_node(&head, 123, NULL);
    TEST_ASSERT_EQUAL(-1, result);
}

void test_insert_empty_key(void) {
    const char *key = "";
    uint32_t hash = 0;
    unsigned char data[] = "empty";
    list_node *head = NULL;
    size_t data_size = sizeof(data);
    data_node *dnode = create_data_node(key, hash, data, data_size);
    int result = insert_list_node(&head, hash, dnode);
    TEST_ASSERT_EQUAL(-1, result);
    list_node *found = find_list_node(head, key, hash);
    TEST_ASSERT_NULL(found);
    delete_list_node(&head, key, hash);
}

void test_insert_large_key(void) {
    char key[1024];
    memset(key, 'A', sizeof(key) - 1);
    key[1023] = '\0';
    uint32_t hash = 123456;
    unsigned char data[] = "large";
    size_t data_size = sizeof(data);
    list_node *head = NULL;

    data_node *dnode = create_data_node(key, hash, data, data_size);
    int result = insert_list_node(&head, hash, dnode);
    TEST_ASSERT_EQUAL(0, result);
    list_node *found = find_list_node(head, key, hash);
    TEST_ASSERT_NOT_NULL(found);
    delete_list_node(&head, key, hash);
}

void test_delete_single_node_list(void) {
    const char *key = "single";
    uint32_t hash = 1;
    unsigned char data[] = "one";
    size_t data_size = sizeof(data);
    list_node *head = NULL;
    data_node *dnode = create_data_node(key, hash, data, data_size);
    int result = insert_list_node(&head, hash, dnode);
    TEST_ASSERT_EQUAL(0, result);
    result = delete_list_node(&head, key, hash);
    TEST_ASSERT_EQUAL(0, result);
}

void test_repeated_insert_delete(void) {
    const char *key = "repeat";
    uint32_t hash = 99;
    unsigned char data[] = "val";
    size_t data_size = sizeof(data);
    list_node *head = NULL;
    for (int i = 0; i < 10; ++i) {
        data_node *dnode = create_data_node(key, hash, data, data_size);
        int result = insert_list_node(&head, hash, dnode);
        TEST_ASSERT_EQUAL(0, result);
        result = delete_list_node(&head, key, hash);
        TEST_ASSERT_EQUAL(0, result);
    }
}

void test_insert_null_key(void) {
    list_node *head = NULL;
    unsigned char *data = NULL;
    size_t data_size = 0;
    data_node *dnode = create_data_node(NULL, 123, data, data_size);
    int result = insert_list_node(&head, 123, dnode);
    // Should handle gracefully (depends on your implementation)
    TEST_ASSERT_EQUAL(-1, result);
}

int test_hash_bucket_list_suite(void) {
    initialize_memory_manager((memory_manager_config){ .bucket_size = 10, .pre_allocation_factor = 1.0, .allocate_list_pool = true, .allocate_tree_pool = false });
    printf("Running hash_bucket_list tests...\n");
    RUN_TEST(test_insert_and_find_list_node);
    RUN_TEST(test_delete_list_node_not_found);
    RUN_TEST(test_find_list_node_not_found);
    RUN_TEST(test_insert_multiple_nodes_and_find);
    RUN_TEST(test_delete_head_and_middle_node);
    RUN_TEST(test_insert_null_data);
    RUN_TEST(test_insert_empty_key);
    RUN_TEST(test_insert_large_key);
    RUN_TEST(test_delete_single_node_list);
    RUN_TEST(test_repeated_insert_delete);
    RUN_TEST(test_insert_null_key);
    printf("Completed hash_bucket_list tests.\n");
    cleanup_memory_manager();
    return 0;
}   