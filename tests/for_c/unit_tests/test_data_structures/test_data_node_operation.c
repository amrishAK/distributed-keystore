/**
 * @file test_data_node_operations.c
 * @brief Unit tests for data_node_operation.h in data_structures dir.
 *
 * This file contains comprehensive unit tests for the data node operations API.
 * All type definitions and error codes are included from the type_definitions directory only.
 *
 * Test Scenarios Covered:
 * 1. Successful creation of a data node with valid key-value pair.
 * 2. Creation with invalid arguments (NULL key_value_pair).
 * 3. Editing the value of an existing data node.
 * 4. Reading the value and key from a data node.
 * 5. Deleting a data node (including NULL pointer case).
 * 6. Soft deleting a data node and verifying the is_deleted flag.
 * 7. Edge cases for memory allocation failures and empty values (if error codes are defined).
 * 8. Creating a data node with very large and value sizes.
 * 9. Editing a data node with very large value sizes.
 * 10. Double soft deletion of a data node (should handle gracefully).
 * 11. Double deletion of a data node (should handle gracefully).
 * 12. Create data node with special character values.
 * 13. Create data node with binary data values.
 * 14. Create data with null output pointer.
 * 15. Edit data node with null new_kv_pair.
 *
 * All tests use the Unity framework for assertions.
 */
#include "unity.h"
#include "data_structures/data_node_operation.h"
#include "type_definitions/hash_bucket_type_definition.h"
#include "type_definitions/error_code_definitions.h"
#include "type_definitions/stats_type_definitions.h"
#include <string.h>
#include <stdlib.h>


void test_create_new_data_node_success(void) {
    key_value_pair kv;
    kv.key = "testkey";
    kv.value = (unsigned char *)"testvalue";
    kv.value_size = strlen("testvalue");
    data_node *node = NULL;
    int result = create_new_data_node(12345, &kv, false, &node);
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_NOT_NULL(node);
    TEST_ASSERT_EQUAL_STRING("testkey", node->key);
    TEST_ASSERT_EQUAL_MEMORY("testvalue", node->data, kv.value_size);
    TEST_ASSERT_EQUAL(kv.value_size, node->data_size);
    delete_data_node(node);
}

void test_create_new_data_node_invalid_args(void) {
    data_node *node = NULL;
    int result = create_new_data_node(12345, NULL, false, &node);
    TEST_ASSERT_LESS_THAN(0, result);
    TEST_ASSERT_NULL(node);
}

void test_create_new_data_node_empty_key(void) {
    key_value_pair kv;
    kv.key = "";
    kv.value = (unsigned char *)"value";
    kv.value_size = strlen("value");
    data_node *node = NULL;
    int result = create_new_data_node(12345, &kv, false, &node);
    TEST_ASSERT_LESS_THAN(0, result);
    TEST_ASSERT_NULL(node);
}

void test_create_new_data_node_null_value(void) {
    key_value_pair kv;
    kv.key = "key";
    kv.value = NULL;
    kv.value_size = 0;
    data_node *node = NULL;
    int result = create_new_data_node(12345, &kv, false, &node);
    TEST_ASSERT_LESS_THAN(0, result);
    TEST_ASSERT_NULL(node);
}

void test_edit_data_node_value(void) {
    key_value_pair kv;
    kv.key = "editkey";
    kv.value = (unsigned char *)"oldvalue";
    kv.value_size = strlen("oldvalue");
    data_node *node = NULL;
    create_new_data_node(54321, &kv, false, &node);
    key_value_pair new_kv;
    new_kv.value = (unsigned char *)"newvalue";
    new_kv.value_size = strlen("newvalue");
    int result = edit_data_node_value(node, &new_kv);
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_EQUAL_MEMORY("newvalue", node->data, new_kv.value_size);
    delete_data_node(node);
}

void test_edit_data_node_value_null_node(void) {
    key_value_pair new_kv;
    new_kv.value = (unsigned char *)"newvalue";
    new_kv.value_size = strlen("newvalue");
    int result = edit_data_node_value(NULL, &new_kv);
    TEST_ASSERT_LESS_THAN(0, result);
}

void test_edit_data_node_value_null_value(void) {
    key_value_pair kv;
    kv.key = "editkey";
    kv.value = (unsigned char *)"oldvalue";
    kv.value_size = strlen("oldvalue");
    data_node *node = NULL;
    create_new_data_node(54321, &kv, false, &node);
    key_value_pair new_kv;
    new_kv.value = NULL;
    new_kv.value_size = 0;
    int result = edit_data_node_value(node, &new_kv);
    TEST_ASSERT_LESS_THAN(0, result);
    delete_data_node(node);
}

void test_read_data_node_value(void) {
    key_value_pair kv;
    kv.key = "readkey";
    kv.value = (unsigned char *)"readvalue";
    kv.value_size = strlen("readvalue");
    data_node *node = NULL;
    create_new_data_node(11111, &kv, false, &node);
    key_value_pair out_kv = {0};
    int result = read_data_node_value(node, &out_kv);
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_EQUAL_STRING("readkey", out_kv.key);
    TEST_ASSERT_EQUAL_MEMORY("readvalue", out_kv.value, kv.value_size);
    TEST_ASSERT_EQUAL(kv.value_size, out_kv.value_size);
    free(out_kv.key);
    free(out_kv.value);
    delete_data_node(node);
}

void test_read_data_node_value_null_node(void) {
    key_value_pair out_kv = {0};
    int result = read_data_node_value(NULL, &out_kv);
    TEST_ASSERT_LESS_THAN(0, result);
}

void test_read_data_node_value_null_out(void) {
    key_value_pair kv;
    kv.key = "readkey";
    kv.value = (unsigned char *)"readvalue";
    kv.value_size = strlen("readvalue");
    data_node *node = NULL;
    create_new_data_node(11111, &kv, false, &node);
    int result = read_data_node_value(node, NULL);
    TEST_ASSERT_LESS_THAN(0, result);
    delete_data_node(node);
}

void test_delete_data_node_null(void) {
    int result = delete_data_node(NULL);
    TEST_ASSERT_EQUAL(0, result);
}

void test_soft_delete_data_node(void) {
    key_value_pair kv;
    kv.key = "softkey";
    kv.value = (unsigned char *)"softvalue";
    kv.value_size = strlen("softvalue");
    data_node *node = NULL;
    create_new_data_node(22222, &kv, false, &node);
    int result = soft_delete_data_node(node);
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_TRUE(node->is_deleted);
    delete_data_node(node);
}

void test_soft_delete_data_node_null(void) {
    int result = soft_delete_data_node(NULL);
    TEST_ASSERT_LESS_THAN(0, result);
}

void test_create_new_data_node_with_large_value(void) {
    key_value_pair kv;
    kv.key = "largekey";
    size_t large_size = 1024 * 1024; // 1 MB
    kv.value = (unsigned char *)malloc(large_size);
    memset(kv.value, 'A', large_size);
    kv.value_size = large_size;
    data_node *node = NULL;
    int result = create_new_data_node(33333, &kv, false, &node);
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_NOT_NULL(node);
    TEST_ASSERT_EQUAL_MEMORY(kv.value, node->data, large_size);
    TEST_ASSERT_EQUAL(large_size, node->data_size);
    delete_data_node(node);
    free(kv.value);
}

void test_edit_data_node_value_with_larger_value(void) {
    key_value_pair kv;
    kv.key = "editlargekey";
    kv.value = (unsigned char *)"smallvalue";
    kv.value_size = strlen("smallvalue");
    data_node *node = NULL;
    create_new_data_node(44444, &kv, false, &node);
    key_value_pair new_kv;
    size_t large_size = 512 * 1024; // 512 KB
    new_kv.value = (unsigned char *)malloc(large_size);
    memset(new_kv.value, 'B', large_size);
    new_kv.value_size = large_size;
    int result = edit_data_node_value(node, &new_kv);
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_EQUAL_MEMORY(new_kv.value, node->data, large_size);
    TEST_ASSERT_EQUAL(large_size, node->data_size);
    delete_data_node(node);
    free(new_kv.value);
}

void test_soft_delete_data_node_already_soft_deleted(void) {
    key_value_pair kv;
    kv.key = "softkey2";
    kv.value = (unsigned char *)"softvalue2";
    kv.value_size = strlen("softvalue2");
    data_node *node = NULL;
    create_new_data_node(55555, &kv, false, &node);
    soft_delete_data_node(node);
    int result = soft_delete_data_node(node);
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_TRUE(node->is_deleted);
    delete_data_node(node);
}

void test_delete_data_node_already_deleted(void) {
    key_value_pair kv;
    kv.key = "delkey";
    kv.value = (unsigned char *)"delvalue";
    kv.value_size = strlen("delvalue");
    data_node *node = NULL;
    create_new_data_node(66666, &kv, false, &node);
    delete_data_node(node);
    node = NULL; // Simulate that node is already deleted
    int result = delete_data_node(node);
    TEST_ASSERT_EQUAL(0, result);
}

void test_create_new_data_node_with_special_char_value(void) {
    key_value_pair kv;
    kv.key = "specialValueKey";
    unsigned char special_value[] = {0x00, 0xFF, 0x7E, 0x81, 0x42};
    kv.value = special_value;
    kv.value_size = sizeof(special_value);
    data_node *node = NULL;
    int result = create_new_data_node(77777, &kv, false, &node);
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_NOT_NULL(node);
    TEST_ASSERT_EQUAL_MEMORY(special_value, node->data, kv.value_size);
    TEST_ASSERT_EQUAL(kv.value_size, node->data_size);
    delete_data_node(node);
}

void test_create_new_data_node_with_binary_value(void) {
    key_value_pair kv;
    kv.key = "binaryKey";
    unsigned char binary_value[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01, 0x02};
    kv.value = binary_value;
    kv.value_size = sizeof(binary_value);
    data_node *node = NULL;
    int result = create_new_data_node(88888, &kv, false, &node);
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_NOT_NULL(node);
    TEST_ASSERT_EQUAL_MEMORY(binary_value, node->data, kv.value_size);
    TEST_ASSERT_EQUAL(kv.value_size, node->data_size);
    delete_data_node(node);
}

void test_create_new_data_node_with_null_out_pointer(void) {
    key_value_pair kv;
    kv.key = "nullOutKey";
    kv.value = (unsigned char *)"nullOutValue";
    kv.value_size = strlen("nullOutValue");
    int result = create_new_data_node(99999, &kv, false, NULL);
    TEST_ASSERT_LESS_THAN(0, result);
}

void test_edit_data_node_with_null_kv_pair(void) {
    key_value_pair kv;
    kv.key = "editNullKey";
    kv.value = (unsigned char *)"editNullValue";
    kv.value_size = strlen("editNullValue");
    data_node *node = NULL;
    create_new_data_node(10101, &kv, false, &node);
    int result = edit_data_node_value(node, NULL);
    TEST_ASSERT_LESS_THAN(0, result);
    delete_data_node(node);
}

int test_data_node_operations_main(void) {
    UNITY_BEGIN();
    printf("Running data_node_operation tests...\n");
    RUN_TEST(test_create_new_data_node_success);
    RUN_TEST(test_create_new_data_node_invalid_args);
    RUN_TEST(test_edit_data_node_value);
    RUN_TEST(test_read_data_node_value);
    RUN_TEST(test_delete_data_node_null);
    RUN_TEST(test_soft_delete_data_node);
    RUN_TEST(test_create_new_data_node_empty_key);
    RUN_TEST(test_create_new_data_node_null_value);
    RUN_TEST(test_edit_data_node_value_null_node);
    RUN_TEST(test_edit_data_node_value_null_value);
    RUN_TEST(test_read_data_node_value_null_node);
    RUN_TEST(test_read_data_node_value_null_out);
    RUN_TEST(test_soft_delete_data_node_null);
    RUN_TEST(test_create_new_data_node_with_large_value);
    RUN_TEST(test_edit_data_node_value_with_larger_value);
    RUN_TEST(test_soft_delete_data_node_already_soft_deleted);
    RUN_TEST(test_delete_data_node_already_deleted);
    RUN_TEST(test_create_new_data_node_with_special_char_value);
    RUN_TEST(test_create_new_data_node_with_binary_value);
    RUN_TEST(test_create_new_data_node_with_null_out_pointer);
    RUN_TEST(test_edit_data_node_with_null_kv_pair);
    printf("data_node_operation tests completed.\n");
    return UNITY_END();
}
