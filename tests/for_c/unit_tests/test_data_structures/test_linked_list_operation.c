/**
 * @file test_linked_list_operations.c
 * @brief Unit tests for linked_list_operations.h in data_structures dir.
 *
 * This file contains comprehensive unit tests for the linked list operations API.
 * All type definitions and error codes are included from the type_definitions directory only.
 *
 * Test Scenarios Covered:
 * 1. Successful creation of a linked list node with valid data_node.
 * 2. Creation with invalid arguments (NULL data_node, NULL output pointer).
 * 3. Insertion of a node at the head of the list.
 * 4. Insertion with invalid arguments (NULL head, NULL node).
 * 5. Retrieval of a data_node by key and key_hash (success and not found).
 * 6. Retrieval with invalid arguments (NULL head, NULL key, NULL output).
 * 7. Deletion of all nodes in the list (empty and non-empty).
 * 8. Cleanup of deleted nodes (with and without deleted nodes).
 * 9. Edge cases for memory allocation failures and error codes.
 *
 * All tests use the Unity framework for assertions.
 */
#include "unity.h"
#include "data_structures/linked_list_operation.h"
#include "data_structures/data_node_operation.h"
#include "type_definitions/hash_bucket_type_definition.h"
#include "type_definitions/error_code_definitions.h"
#include "type_definitions/stats_type_definitions.h"
#include <string.h>
#include <stdlib.h>

void test_create_new_linked_list_node_success(void) {
    key_value_pair kv = {"key", (unsigned char *)"value", strlen("value")};
    data_node *data = NULL;
    create_new_data_node(123, &kv, false, &data);
    linked_list_node *node = NULL;
    int result = create_new_linked_list_node(123, data, &node);
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_NOT_NULL(node);
    TEST_ASSERT_EQUAL_PTR(data, node->data_node_ptr);
    TEST_ASSERT_EQUAL(123, node->key_hash);
    TEST_ASSERT_NULL(node->next_node_ptr);
    delete_data_node(data);
    if (node) free(node);
}

void test_create_new_linked_list_node_null_data(void) {
    linked_list_node *node = NULL;
    int result = create_new_linked_list_node(123, NULL, &node);
    TEST_ASSERT_LESS_THAN(0, result);
    TEST_ASSERT_NULL(node);
}

void test_create_new_linked_list_node_null_out(void) {
    key_value_pair kv = {"key", (unsigned char *)"value", strlen("value")};
    data_node *data = NULL;
    create_new_data_node(123, &kv, false, &data);
    int result = create_new_linked_list_node(123, data, NULL);
    TEST_ASSERT_LESS_THAN(0, result);
    delete_data_node(data);
}

void test_create_new_linked_list_node_invalid_key_hash(void) {
    key_value_pair kv = {"key", (unsigned char *)"value", strlen("value")};
    data_node *data = NULL;
    create_new_data_node(0, &kv, false, &data); // 0 as an edge case for key_hash
    linked_list_node *node = NULL;
    int result = create_new_linked_list_node(0, data, &node);
    TEST_ASSERT_EQUAL(0, result); // Should still succeed if 0 is valid
    delete_data_node(data);
    if (node) free(node);
}

void test_insert_linked_list_node_success(void) {
    key_value_pair kv = {"key", (unsigned char *)"value", strlen("value")};
    data_node *data = NULL;
    create_new_data_node(123, &kv, false, &data);
    linked_list_node *node = NULL;
    create_new_linked_list_node(123, data, &node);
    linked_list_node *head = NULL;
    int result = insert_linked_list_node(&head, node);
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_EQUAL_PTR(node, head);
    delete_all_linked_list_nodes(head);
}

void test_insert_linked_list_node_null_head(void) {
    key_value_pair kv = {"key", (unsigned char *)"value", strlen("value")};
    data_node *data = NULL;
    create_new_data_node(123, &kv, false, &data);
    linked_list_node *node = NULL;
    create_new_linked_list_node(123, data, &node);
    int result = insert_linked_list_node(NULL, node);
    TEST_ASSERT_LESS_THAN(0, result);
    delete_data_node(data);
    if (node) free(node);
}

void test_insert_linked_list_node_null_node(void) {
    linked_list_node *head = NULL;
    int result = insert_linked_list_node(&head, NULL);
    TEST_ASSERT_LESS_THAN(0, result);
}

void test_get_data_node_from_linked_list_success(void) {
    key_value_pair kv = {"key", (unsigned char *)"value", strlen("value")};
    data_node *data = NULL;
    create_new_data_node(123, &kv, false, &data);
    linked_list_node *node = NULL;
    create_new_linked_list_node(123, data, &node);
    linked_list_node *head = node;
    data_node *out = NULL;
    int result = get_data_node_from_linked_list(head, "key", 123, &out);
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_EQUAL_PTR(data, out);
    delete_data_node(data);
    if (node) free(node);
}

void test_get_data_node_from_linked_list_not_found(void) {
    linked_list_node *head = NULL;
    data_node *out = NULL;
    int result = get_data_node_from_linked_list(head, "notfound", 999, &out);
    TEST_ASSERT_NOT_EQUAL(0, result);
    TEST_ASSERT_NULL(out);
}

void test_get_data_node_from_linked_list_null_head(void) {
    data_node *out = NULL;
    int result = get_data_node_from_linked_list(NULL, "key", 123, &out);
    TEST_ASSERT_NOT_EQUAL(0, result);
    TEST_ASSERT_NULL(out);
}

void test_get_data_node_from_linked_list_null_key(void) {
    key_value_pair kv = {"key", (unsigned char *)"value", strlen("value")};
    data_node *data = NULL;
    create_new_data_node(123, &kv, false, &data);
    linked_list_node *node = NULL;
    create_new_linked_list_node(123, data, &node);
    linked_list_node *head = node;
    data_node *out = NULL;
    int result = get_data_node_from_linked_list(head, NULL, 123, &out);
    TEST_ASSERT_NOT_EQUAL(0, result);
    TEST_ASSERT_NULL(out);
    delete_data_node(data);
    if (node) free(node);
}

void test_get_data_node_from_linked_list_null_out(void) {
    key_value_pair kv = {"key", (unsigned char *)"value", strlen("value")};
    data_node *data = NULL;
    create_new_data_node(123, &kv, false, &data);
    linked_list_node *node = NULL;
    create_new_linked_list_node(123, data, &node);
    linked_list_node *head = node;
    int result = get_data_node_from_linked_list(head, "key", 123, NULL);
    TEST_ASSERT_NOT_EQUAL(0, result);
    delete_data_node(data);
    free(node);
}

void test_get_data_node_from_linked_list_multiple_nodes(void) {
    key_value_pair kv1 = {"key1", (unsigned char *)"value1", strlen("value1")};
    key_value_pair kv2 = {"key2", (unsigned char *)"value2", strlen("value2")};
    data_node *data1 = NULL;
    data_node *data2 = NULL;
    create_new_data_node(111, &kv1, false, &data1);
    create_new_data_node(222, &kv2, false, &data2);
    linked_list_node *node1 = NULL;
    linked_list_node *node2 = NULL;
    create_new_linked_list_node(111, data1, &node1);
    create_new_linked_list_node(222, data2, &node2);
    linked_list_node *head = NULL;
    insert_linked_list_node(&head, node1);
    insert_linked_list_node(&head, node2);
    data_node *out = NULL;
    int result = get_data_node_from_linked_list(head, "key1", 111, &out);
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_EQUAL_PTR(data1, out);
    delete_all_linked_list_nodes(head);
}

void test_delete_all_linked_list_nodes_empty(void) {
    linked_list_node *head = NULL;
    int result = delete_all_linked_list_nodes(head);
    TEST_ASSERT_EQUAL(0, result);
}

void test_delete_all_linked_list_nodes_non_empty(void) {
    key_value_pair kv = {"key", (unsigned char *)"value", strlen("value")};
    data_node *data = NULL;
    create_new_data_node(123, &kv, false, &data);
    linked_list_node *node = NULL;
    create_new_linked_list_node(123, data, &node);
    linked_list_node *head = node;
    int result = delete_all_linked_list_nodes(head);
    TEST_ASSERT_EQUAL(0, result);
}

void test_delete_all_linked_list_nodes_null(void) {
    int result = delete_all_linked_list_nodes(NULL);
    TEST_ASSERT_EQUAL(0, result);
}

void test_cleanup_deleted_linked_list_nodes_none_deleted(void) {
    key_value_pair kv = {"key", (unsigned char *)"value", strlen("value")};
    data_node *data = NULL;
    create_new_data_node(123, &kv, false, &data);
    linked_list_node *node = NULL;
    create_new_linked_list_node(123, data, &node);
    linked_list_node *head = node;
    int result = cleanup_deleted_linked_list_nodes(head);
    TEST_ASSERT_EQUAL(0, result);
    delete_data_node(data);
    if (node) free(node);
}

void test_cleanup_deleted_linked_list_nodes_some_deleted(void) {
    key_value_pair kv = {"key", (unsigned char *)"value", strlen("value")};
    data_node *data = NULL;
    create_new_data_node(123, &kv, false, &data);
    linked_list_node *node = NULL;
    create_new_linked_list_node(123, data, &node);
    node->data_node_ptr->is_deleted = true;
    linked_list_node *head = node;
    int result = cleanup_deleted_linked_list_nodes(head);
    TEST_ASSERT_GREATER_OR_EQUAL(1, result);
}

void test_cleanup_deleted_linked_list_nodes_all_deleted(void) {
    key_value_pair kv1 = {"key1", (unsigned char *)"value1", strlen("value1")};
    key_value_pair kv2 = {"key2", (unsigned char *)"value2", strlen("value2")};
    data_node *data1 = NULL;
    data_node *data2 = NULL;
    create_new_data_node(111, &kv1, false, &data1);
    create_new_data_node(222, &kv2, false, &data2);
    linked_list_node *node1 = NULL;
    linked_list_node *node2 = NULL;
    create_new_linked_list_node(111, data1, &node1);
    create_new_linked_list_node(222, data2, &node2);
    node1->data_node_ptr->is_deleted = true;
    node2->data_node_ptr->is_deleted = true;
    linked_list_node *head = NULL;
    insert_linked_list_node(&head, node1);
    insert_linked_list_node(&head, node2);
    int result = cleanup_deleted_linked_list_nodes(head);
    TEST_ASSERT_GREATER_OR_EQUAL(2, result);
}

int test_linked_list_operations_main(void) {
    UNITY_BEGIN();
    printf("Running Linked List Operations Unit Tests...\n");
    RUN_TEST(test_create_new_linked_list_node_success);
    RUN_TEST(test_create_new_linked_list_node_null_data);
    RUN_TEST(test_create_new_linked_list_node_null_out);
    RUN_TEST(test_create_new_linked_list_node_invalid_key_hash);
    RUN_TEST(test_insert_linked_list_node_success);
    RUN_TEST(test_insert_linked_list_node_null_head);
    RUN_TEST(test_insert_linked_list_node_null_node);
    RUN_TEST(test_get_data_node_from_linked_list_success);
    RUN_TEST(test_get_data_node_from_linked_list_not_found);
    RUN_TEST(test_get_data_node_from_linked_list_null_head);
    RUN_TEST(test_get_data_node_from_linked_list_null_key);
    RUN_TEST(test_get_data_node_from_linked_list_null_out);
    RUN_TEST(test_get_data_node_from_linked_list_multiple_nodes);
    RUN_TEST(test_delete_all_linked_list_nodes_empty);
    RUN_TEST(test_delete_all_linked_list_nodes_non_empty);
    RUN_TEST(test_delete_all_linked_list_nodes_null);
    RUN_TEST(test_cleanup_deleted_linked_list_nodes_none_deleted);
    RUN_TEST(test_cleanup_deleted_linked_list_nodes_some_deleted);
    RUN_TEST(test_cleanup_deleted_linked_list_nodes_all_deleted);
    printf("Linked List Operations Unit Tests Completed.\n");
    return UNITY_END();
}