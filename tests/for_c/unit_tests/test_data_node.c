#include "unity.h"
#include "core/data_node.h"
#include <string.h>

void test_create_data_node(void) {
    const char *key = "mykey";
    uint32_t key_hash = 12345;
    unsigned char data[] = "value";
    size_t data_size = sizeof(data);

    data_node *node = create_data_node(key, key_hash, data, data_size);
    TEST_ASSERT_NOT_NULL(node);
    TEST_ASSERT_EQUAL_STRING(key, node->key);
    TEST_ASSERT_EQUAL_UINT32(key_hash, node->key_hash);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(data, node->data, data_size);
    TEST_ASSERT_EQUAL(data_size, node->data_size);

    delete_data_node(node);
}

void test_update_data_node(void) {
    const char *key = "mykey";
    uint32_t key_hash = 12345;
    unsigned char data[] = "value";
    size_t data_size = sizeof(data);

    data_node *node = create_data_node(key, key_hash, data, data_size);
    TEST_ASSERT_NOT_NULL(node);

    unsigned char new_data[] = "newval";
    size_t new_data_size = sizeof(new_data);

    int result = update_data_node(node, new_data, new_data_size);
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(new_data, node->data, new_data_size);
    TEST_ASSERT_EQUAL(new_data_size, node->data_size);

    delete_data_node(node);
}

void test_delete_data_node_null(void) {
    int result = delete_data_node(NULL);
    TEST_ASSERT_EQUAL(-1, result);
}


void test_create_data_node_null_params(void) {
    data_node *node = create_data_node(NULL, 0, NULL, 0);
    TEST_ASSERT_NULL(node);
}

void test_update_data_node_null_params(void) {
    int result = update_data_node(NULL, NULL, 0);
    TEST_ASSERT_EQUAL(-1, result);

    unsigned char data[] = "abc";
    data_node *node = create_data_node("key", 1, data, sizeof(data));
    result = update_data_node(node, NULL, 0);
    TEST_ASSERT_EQUAL(-1, result);
    delete_data_node(node);
}

void test_update_data_node_size_zero(void) {
    unsigned char data[] = "abc";
    data_node *node = create_data_node("key", 1, data, sizeof(data));
    unsigned char new_data[] = "";
    size_t new_data_size = 0;
    int result = update_data_node(node, new_data, new_data_size);
    TEST_ASSERT_EQUAL(0, result);
    delete_data_node(node);
}

void test_update_data_node_with_empty_data(void) {
    unsigned char data[] = "abc";
    data_node *node = create_data_node("key", 1, data, sizeof(data));
    unsigned char new_data[] = "";
    size_t new_data_size = 0;
    int result = update_data_node(node, new_data, new_data_size);
    TEST_ASSERT_EQUAL(0, result);
    delete_data_node(node);
}

void test_update_data_node_with_bigger_data(void) {
    unsigned char data[] = "abc";
    data_node *node = create_data_node("key", 1, data, sizeof(data));
    unsigned char new_data[] = "abcdefabcdef";
    size_t new_data_size = sizeof(new_data);
    int result = update_data_node(node, new_data, new_data_size);
    TEST_ASSERT_EQUAL(0, result);
    delete_data_node(node);
}

void test_update_data_node_with_smaller_data(void) {
    unsigned char data[] = "abcabcabc";
    data_node *node = create_data_node("key", 1, data, sizeof(data));
    unsigned char new_data[] = "ab";
    size_t new_data_size = sizeof(new_data);
    int result = update_data_node(node, new_data, new_data_size);
    TEST_ASSERT_EQUAL(0, result);
    delete_data_node(node);
}

void test_update_data_node_with_same_size_data(void) {
    unsigned char data[] = "abcabcabc";
    data_node *node = create_data_node("key", 1, data, sizeof(data));
    unsigned char new_data[] = "abcabcabc";
    size_t new_data_size = sizeof(new_data);
    int result = update_data_node(node, new_data, new_data_size);
    TEST_ASSERT_EQUAL(0, result);
    delete_data_node(node);
}

void test_delete_data_node_valid(void) {
    unsigned char data[] = "abc";
    data_node *node = create_data_node("key", 1, data, sizeof(data));
    int result = delete_data_node(node);
    TEST_ASSERT_EQUAL(0, result);
}

int test_data_node_suite(void) {
    RUN_TEST(test_create_data_node);
    RUN_TEST(test_update_data_node);
    RUN_TEST(test_delete_data_node_null);
    RUN_TEST(test_create_data_node_null_params);
    RUN_TEST(test_update_data_node_null_params);
    RUN_TEST(test_update_data_node_size_zero);
    RUN_TEST(test_update_data_node_with_empty_data);
    RUN_TEST(test_update_data_node_with_bigger_data);
    RUN_TEST(test_update_data_node_with_smaller_data);
    RUN_TEST(test_update_data_node_with_same_size_data);
    RUN_TEST(test_delete_data_node_valid);
    return 0;
}