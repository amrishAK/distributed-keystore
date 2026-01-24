/**
 * @file test_sub_hash_bucker_operation.c
 * @brief Unit tests for sub-hash-bucket operations.
 * This file contains comprehensive unit tests for the sub-hash-bucket operations API.
 * Test Scenarios Covered:
 * 1. Initialise a new sub-hash-bucket successfully.
 * 2. Fail to initialise a sub-hash-bucket with invalid arguments.
 * 3. Cleanup a sub-hash-bucket successfully.
 * 4. Fail to cleanup a sub-hash-bucket with invalid arguments.
 * 5. Add a node to the sub-hash-bucket and retrieve it.
 * 6. Update a node in the sub-hash-bucket.
 * 7. Delete a key from the sub-hash-bucket.
 * 8. Attempt to delete a nonexistent key (should fail gracefully).
 * 9. Cleanup of a non-empty bucket.
 * 10. Add/update/get/delete with invalid arguments.
 * 11. Add multiple nodes (collision/chain scenario).
 * All tests use the Unity framework for assertions.
 */
#include "unity.h"
#include "sub_hash_table/sub_hash_bucket_operation.h"
#include "type_definitions/hash_bucket_type_definition.h"
#include "type_definitions/error_code_definitions.h"
#include <string.h>
#include <stdlib.h>

void test_initialise_sub_hash_bucket_success(void) {
    sub_hash_bucket bucket;
    int result = initialise_sub_hash_bucket(&bucket, false, 4);
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_TRUE(bucket.is_initialized);
    TEST_ASSERT_NULL(bucket.linked_list_head);
    TEST_ASSERT_EQUAL(0, bucket.active_node_count);
    cleanup_sub_hash_bucket(&bucket);
}

void test_initialise_sub_hash_bucket_null(void) {
    int result = initialise_sub_hash_bucket(NULL, false, 4);
    TEST_ASSERT_EQUAL(ERR_INVALID_ARGUMENT, result);
}

void test_cleanup_sub_hash_bucket_null(void) {
    int result = cleanup_sub_hash_bucket(NULL);
    TEST_ASSERT_EQUAL(ERR_INVALID_ARGUMENT, result);
}

void test_add_and_get_node_from_sub_hash_bucket(void) {
    sub_hash_bucket bucket;
    initialise_sub_hash_bucket(&bucket, false, 4);
    unsigned char value[] = "value";
    key_value_pair kv = {"key", value, strlen((char*)value) + 1};
    sub_hash_bucket_operation_args args = {&bucket, "key", 123};
    int add_result = add_node_to_sub_hash_bucket(args, &kv);
    TEST_ASSERT_TRUE(add_result == 0 || add_result == 20);
    key_value_pair out = {0};
    int get_result = get_key_store_value_from_sub_hash_bucket(args, &out);
    TEST_ASSERT_EQUAL(0, get_result);
    TEST_ASSERT_EQUAL_STRING("key", out.key);
    TEST_ASSERT_EQUAL_STRING("value", out.value);
    cleanup_sub_hash_bucket(&bucket);
}

void test_update_node_in_sub_hash_bucket(void) {
    sub_hash_bucket bucket;
    initialise_sub_hash_bucket(&bucket, false, 4);
    unsigned char value[] = "value";
    key_value_pair kv = {"key", value, strlen((char*)value) + 1};
    sub_hash_bucket_operation_args args = {&bucket, "key", 123};
    add_node_to_sub_hash_bucket(args, &kv);
    unsigned char new_value[] = "newval";
    key_value_pair new_kv = {"key", new_value, strlen((char*)new_value) + 1};
    int update_result = update_node_in_sub_hash_bucket(args, &new_kv);
    TEST_ASSERT_EQUAL(0, update_result);
    key_value_pair out = {0};
    int get_result = get_key_store_value_from_sub_hash_bucket(args, &out);
    TEST_ASSERT_EQUAL(0, get_result);
    TEST_ASSERT_EQUAL_STRING("newval", out.value);
    cleanup_sub_hash_bucket(&bucket);
}

void test_delete_key_from_sub_hash_bucket(void) {
    sub_hash_bucket bucket;
    initialise_sub_hash_bucket(&bucket, false, 4);
    unsigned char value[] = "value";
    key_value_pair kv = {"key", value, strlen((char*)value) + 1};
    sub_hash_bucket_operation_args args = {&bucket, "key", 123};
    add_node_to_sub_hash_bucket(args, &kv);
    int del_result = delete_key_from_sub_hash_bucket(args);
    TEST_ASSERT_EQUAL(0, del_result);
    key_value_pair out = {0};
    int get_result = get_key_store_value_from_sub_hash_bucket(args, &out);
    TEST_ASSERT_LESS_THAN(0, get_result);
    cleanup_sub_hash_bucket(&bucket);
}

void test_delete_nonexistent_key_from_sub_hash_bucket(void) {
    sub_hash_bucket bucket;
    initialise_sub_hash_bucket(&bucket, false, 4);
    sub_hash_bucket_operation_args args = {&bucket, "nope", 999};
    int del_result = delete_key_from_sub_hash_bucket(args);
    TEST_ASSERT_LESS_THAN(0, del_result);
    cleanup_sub_hash_bucket(&bucket);
}

void test_cleanup_non_empty_sub_hash_bucket(void) {
    sub_hash_bucket bucket;
    initialise_sub_hash_bucket(&bucket, false, 4);
    unsigned char v1[] = "v1";
    unsigned char v2[] = "v2";
    key_value_pair kv1 = {"a", v1, strlen((char*)v1) + 1};
    key_value_pair kv2 = {"b", v2, strlen((char*)v2) + 1};
    sub_hash_bucket_operation_args args1 = {&bucket, "a", 1};
    sub_hash_bucket_operation_args args2 = {&bucket, "b", 2};
    add_node_to_sub_hash_bucket(args1, &kv1);
    add_node_to_sub_hash_bucket(args2, &kv2);
    int res = cleanup_sub_hash_bucket(&bucket);
    TEST_ASSERT_EQUAL(0, res);
}

void test_invalid_args_sub_hash_bucket(void) {
    unsigned char v[] = "v";
    key_value_pair kv = {"k", v, strlen((char*)v) + 1};
    sub_hash_bucket_operation_args args = {NULL, NULL, 0};
    TEST_ASSERT_LESS_THAN(0, add_node_to_sub_hash_bucket(args, NULL));
    TEST_ASSERT_LESS_THAN(0, add_node_to_sub_hash_bucket(args, &kv));
    TEST_ASSERT_LESS_THAN(0, update_node_in_sub_hash_bucket(args, NULL));
    TEST_ASSERT_LESS_THAN(0, update_node_in_sub_hash_bucket(args, &kv));
    TEST_ASSERT_LESS_THAN(0, get_key_store_value_from_sub_hash_bucket(args, NULL));
    TEST_ASSERT_LESS_THAN(0, get_key_store_value_from_sub_hash_bucket(args, &kv));
    TEST_ASSERT_LESS_THAN(0, delete_key_from_sub_hash_bucket(args));
}

void test_multiple_nodes_chain_sub_hash_bucket(void) {
    sub_hash_bucket bucket;
    initialise_sub_hash_bucket(&bucket, false, 4);
    unsigned char v1[] = "v1";
    unsigned char v2[] = "v2";
    key_value_pair kv1 = {"A", v1, strlen((char*)v1) + 1};
    key_value_pair kv2 = {"B", v2, strlen((char*)v2) + 1};
    uint32_t fake_hash = 12345;
    sub_hash_bucket_operation_args args1 = {&bucket, "A", fake_hash};
    sub_hash_bucket_operation_args args2 = {&bucket, "B", fake_hash};
    add_node_to_sub_hash_bucket(args1, &kv1);
    add_node_to_sub_hash_bucket(args2, &kv2);
    key_value_pair out1 = {0}, out2 = {0};
    int r1 = get_key_store_value_from_sub_hash_bucket(args1, &out1);
    int r2 = get_key_store_value_from_sub_hash_bucket(args2, &out2);
    TEST_ASSERT_EQUAL(0, r1);
    TEST_ASSERT_EQUAL(0, r2);
    TEST_ASSERT_EQUAL_STRING("A", out1.key);
    TEST_ASSERT_EQUAL_STRING("B", out2.key);
    cleanup_sub_hash_bucket(&bucket);
}

void test_update_delete_get_after_sub_hash_bucket_cleanup(void) {
    sub_hash_bucket bucket;
    initialise_sub_hash_bucket(&bucket, false, 4);
    unsigned char v[] = "v";
    key_value_pair kv = {"gone", v, strlen((char*)v) + 1};
    sub_hash_bucket_operation_args args = {&bucket, "gone", 77};
    add_node_to_sub_hash_bucket(args, &kv);
    cleanup_sub_hash_bucket(&bucket);
    int res1 = update_node_in_sub_hash_bucket(args, &kv);
    int res2 = delete_key_from_sub_hash_bucket(args);
    key_value_pair out = {0};
    int res3 = get_key_store_value_from_sub_hash_bucket(args, &out);
    TEST_ASSERT_LESS_THAN(0, res1);
    TEST_ASSERT_LESS_THAN(0, res2);
    TEST_ASSERT_LESS_THAN(0, res3);
}

void test_repeated_add_update_delete_same_key_sub_hash_bucket(void) {
    sub_hash_bucket bucket;
    initialise_sub_hash_bucket(&bucket, false, 4);
    unsigned char v1[] = "v1";
    unsigned char v2[] = "v2";
    key_value_pair kv = {"repeat", v1, strlen((char*)v1) + 1};
    sub_hash_bucket_operation_args args = {&bucket, "repeat", 55};
    add_node_to_sub_hash_bucket(args, &kv);
    kv.value = v2;
    kv.value_size = strlen((char*)v2) + 1;
    update_node_in_sub_hash_bucket(args, &kv);
    int del1 = delete_key_from_sub_hash_bucket(args);
    int del2 = delete_key_from_sub_hash_bucket(args);
    int add2 = add_node_to_sub_hash_bucket(args, &kv);
    TEST_ASSERT_EQUAL(0, del1);
    TEST_ASSERT_LESS_THAN(0, del2);
    TEST_ASSERT_TRUE(add2 == 0 || add2 == 20);
    cleanup_sub_hash_bucket(&bucket);
}


void test_edit_sub_hash_bucket_node_after_deletion(void) {
    sub_hash_bucket bucket;
    initialise_sub_hash_bucket(&bucket, false, 4);

    //create node
    unsigned char value[] = "value";
    key_value_pair kv = {"key", value, (unsigned int)strlen((char*)value)+1};
    sub_hash_bucket_operation_args args = {&bucket, "key", 55};
    int result = add_node_to_sub_hash_bucket(args, &kv);
    TEST_ASSERT_TRUE(result == 0 || result == 20);
    //delete
    int del_result = delete_key_from_sub_hash_bucket(args);
    TEST_ASSERT_EQUAL(0, del_result);
    //try to update deleted node
    unsigned char new_value[] = "newval";
    key_value_pair new_kv = {"key", new_value, (unsigned int)strlen((char*)new_value)+1};
    int update_result = update_node_in_sub_hash_bucket(args, &new_kv);
    TEST_ASSERT_LESS_THAN(0, update_result);
    cleanup_sub_hash_bucket(&bucket);
}

void test_read_sub_hash_bucket_node_after_deletion(void){
    sub_hash_bucket bucket;
    initialise_sub_hash_bucket(&bucket, false, 4);

    //create node
    unsigned char value[] = "value";
    key_value_pair kv = {"key", value, (unsigned int)strlen((char*)value)+1};
    sub_hash_bucket_operation_args args = {&bucket, "key", 55};
    int result = add_node_to_sub_hash_bucket(args, &kv);
    TEST_ASSERT_TRUE(result == 0 || result == 20);
    //delete
    int del_result = delete_key_from_sub_hash_bucket(args);
    TEST_ASSERT_EQUAL(0, del_result);
    //try to read deleted node
    key_value_pair out = {0};
    int get_result = get_key_store_value_from_sub_hash_bucket(args, &out);
    TEST_ASSERT_LESS_THAN(0, get_result);
    cleanup_sub_hash_bucket(&bucket);
}

void test_delete_sub_hash_bucket_node_twice(void) {
    sub_hash_bucket bucket;
    initialise_sub_hash_bucket(&bucket, false, 4);

    //create node
    unsigned char value[] = "value";
    key_value_pair kv = {"key", value, (unsigned int)strlen((char*)value)+1};
    sub_hash_bucket_operation_args args = {&bucket, "key", 55};
    int result = add_node_to_sub_hash_bucket(args, &kv);
    TEST_ASSERT_TRUE(result == 0 || result == 20);
    //delete first time
    int del_result1 = delete_key_from_sub_hash_bucket(args);
    TEST_ASSERT_EQUAL(0, del_result1);
    //delete second time
    int del_result2 = delete_key_from_sub_hash_bucket(args);
    TEST_ASSERT_LESS_THAN(0, del_result2);
    cleanup_sub_hash_bucket(&bucket);
}

void test_create_sub_hash_bucket_node_with_zero_length_value(void) {
    sub_hash_bucket bucket;
    initialise_sub_hash_bucket(&bucket, false, 4);

    //create node with zero-length value
    unsigned char value[] = "";
    key_value_pair kv = {"key", value, 0};
    sub_hash_bucket_operation_args args = {&bucket, "key", 55};
    int result = add_node_to_sub_hash_bucket(args, &kv);
    TEST_ASSERT_TRUE(result == 0 || result == 20);

    //retrieve and verify
    key_value_pair out = {0};
    int get_result = get_key_store_value_from_sub_hash_bucket(args, &out);
    TEST_ASSERT_EQUAL(0, get_result);
    TEST_ASSERT_EQUAL_STRING("key", out.key);
    TEST_ASSERT_EQUAL(0, out.value_size);

    cleanup_sub_hash_bucket(&bucket);
}

int test_sub_hash_bucket_operation_main(void) {
    UNITY_BEGIN();
    printf("Running Sub Hash Bucket Operation Unit Tests...\n");
    RUN_TEST(test_initialise_sub_hash_bucket_success);
    RUN_TEST(test_initialise_sub_hash_bucket_null);
    RUN_TEST(test_cleanup_sub_hash_bucket_null);
    RUN_TEST(test_add_and_get_node_from_sub_hash_bucket);
    RUN_TEST(test_update_node_in_sub_hash_bucket);
    RUN_TEST(test_delete_key_from_sub_hash_bucket);
    RUN_TEST(test_delete_nonexistent_key_from_sub_hash_bucket);
    RUN_TEST(test_cleanup_non_empty_sub_hash_bucket);
    RUN_TEST(test_invalid_args_sub_hash_bucket);
    RUN_TEST(test_multiple_nodes_chain_sub_hash_bucket);
    RUN_TEST(test_update_delete_get_after_sub_hash_bucket_cleanup);
    RUN_TEST(test_repeated_add_update_delete_same_key_sub_hash_bucket);
    printf("Sub Hash Bucket Operation Unit Tests Completed.\n");
    return UNITY_END();
}


