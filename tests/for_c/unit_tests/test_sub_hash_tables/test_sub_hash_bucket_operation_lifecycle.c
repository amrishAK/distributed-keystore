#include "unity.h"
#include "sub_hash_table/sub_hash_bucket_operation.h"
#include "type_definitions/error_code_definitions.h"
#include "type_definitions/sucess_code_definitions.h"

#include <string.h>

void test_initialise_sub_hash_bucket_success(void) {
    sub_hash_bucket bucket = {0};
    int result = initialise_sub_hash_bucket(&bucket, false, 4);

    TEST_ASSERT_EQUAL_INT(SUCCESS, result);
    TEST_ASSERT_TRUE(bucket.is_initialized);
    TEST_ASSERT_NULL(bucket.linked_list_head);
    TEST_ASSERT_EQUAL_UINT(0, bucket.active_node_count);

    cleanup_sub_hash_bucket(&bucket);
}

void test_initialise_sub_hash_bucket_null_returns_error(void) {
    int result = initialise_sub_hash_bucket(NULL, false, 4);
    TEST_ASSERT_EQUAL_INT(ERR_INVALID_ARGUMENT, result);
}

void test_cleanup_sub_hash_bucket_null_returns_error(void) {
    int result = cleanup_sub_hash_bucket(NULL);
    TEST_ASSERT_EQUAL_INT(ERR_INVALID_ARGUMENT, result);
}

void test_cleanup_non_empty_sub_hash_bucket_succeeds(void) {
    sub_hash_bucket bucket = {0};
    initialise_sub_hash_bucket(&bucket, false, 4);

    unsigned char value_a[] = "v1";
    unsigned char value_b[] = "v2";
    key_value_pair kv_a = {"a", value_a, strlen((char*)value_a) + 1};
    key_value_pair kv_b = {"b", value_b, strlen((char*)value_b) + 1};

    sub_hash_bucket_operation_args args_a = {&bucket, "a", {1, 0}};
    sub_hash_bucket_operation_args args_b = {&bucket, "b", {2, 0}};

    TEST_ASSERT_EQUAL_INT(SUCESS_ADDED_NEW_NODE, add_node_to_sub_hash_bucket(args_a, &kv_a));
    TEST_ASSERT_EQUAL_INT(SUCESS_ADDED_NEW_NODE, add_node_to_sub_hash_bucket(args_b, &kv_b));

    int cleanup_result = cleanup_sub_hash_bucket(&bucket);
    TEST_ASSERT_EQUAL_INT(SUCCESS, cleanup_result);
    TEST_ASSERT_FALSE(bucket.is_initialized);
}

void test_invalid_args_sub_hash_bucket_operations(void) {
    unsigned char value[] = "v";
    key_value_pair kv = {"k", value, strlen((char*)value) + 1};
    sub_hash_bucket_operation_args invalid_args = {NULL, NULL, {0, 0}};

    TEST_ASSERT_LESS_THAN_INT(0, add_node_to_sub_hash_bucket(invalid_args, NULL));
    TEST_ASSERT_LESS_THAN_INT(0, add_node_to_sub_hash_bucket(invalid_args, &kv));
    TEST_ASSERT_LESS_THAN_INT(0, update_node_in_sub_hash_bucket(invalid_args, NULL));
    TEST_ASSERT_LESS_THAN_INT(0, update_node_in_sub_hash_bucket(invalid_args, &kv));
    TEST_ASSERT_LESS_THAN_INT(0, get_key_store_value_from_sub_hash_bucket(invalid_args, NULL));
    TEST_ASSERT_LESS_THAN_INT(0, get_key_store_value_from_sub_hash_bucket(invalid_args, &kv));
    TEST_ASSERT_LESS_THAN_INT(0, delete_key_from_sub_hash_bucket(invalid_args));
}

void test_create_sub_hash_bucket_node_with_zero_length_value_returns_error(void) {
    sub_hash_bucket bucket = {0};
    initialise_sub_hash_bucket(&bucket, false, 4);

    unsigned char value[] = "";
    key_value_pair kv = {"key", value, 0};
    sub_hash_bucket_operation_args args = {&bucket, "key", {55, 0}};

    int result = add_node_to_sub_hash_bucket(args, &kv);
    TEST_ASSERT_LESS_THAN_INT(0, result);

    cleanup_sub_hash_bucket(&bucket);
}

void test_double_initialise_and_cleanup_sub_hash_bucket_is_safe(void) {
    sub_hash_bucket bucket = {0};
    initialise_sub_hash_bucket(&bucket, false, 4);

    int second_init = initialise_sub_hash_bucket(&bucket, false, 4);
    TEST_ASSERT_EQUAL_INT(SUCCESS, second_init);
    TEST_ASSERT_TRUE(bucket.is_initialized);

    TEST_ASSERT_EQUAL_INT(SUCCESS, cleanup_sub_hash_bucket(&bucket));
    TEST_ASSERT_FALSE(bucket.is_initialized);

    TEST_ASSERT_EQUAL_INT(SUCCESS, cleanup_sub_hash_bucket(&bucket));
    TEST_ASSERT_FALSE(bucket.is_initialized);
}

void test_add_node_with_null_key_returns_error(void) {
    sub_hash_bucket bucket = {0};
    initialise_sub_hash_bucket(&bucket, false, 4);

    unsigned char value[] = "value";
    key_value_pair kv = {NULL, value, strlen((char*)value) + 1};
    sub_hash_bucket_operation_args args = {&bucket, NULL, {123, 0}};

    TEST_ASSERT_LESS_THAN_INT(0, add_node_to_sub_hash_bucket(args, &kv));

    cleanup_sub_hash_bucket(&bucket);
}

void test_add_node_with_empty_key_returns_error(void) {
    sub_hash_bucket bucket = {0};
    initialise_sub_hash_bucket(&bucket, false, 4);

    unsigned char value[] = "value";
    key_value_pair kv = {"", value, strlen((char*)value) + 1};
    sub_hash_bucket_operation_args args = {&bucket, "", {123, 0}};

    TEST_ASSERT_LESS_THAN_INT(0, add_node_to_sub_hash_bucket(args, &kv));

    cleanup_sub_hash_bucket(&bucket);
}

int test_sub_hash_bucket_operation_lifecycle_main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_initialise_sub_hash_bucket_success);
    RUN_TEST(test_initialise_sub_hash_bucket_null_returns_error);
    RUN_TEST(test_cleanup_sub_hash_bucket_null_returns_error);
    RUN_TEST(test_cleanup_non_empty_sub_hash_bucket_succeeds);
    RUN_TEST(test_invalid_args_sub_hash_bucket_operations);
    RUN_TEST(test_create_sub_hash_bucket_node_with_zero_length_value_returns_error);
    RUN_TEST(test_double_initialise_and_cleanup_sub_hash_bucket_is_safe);
    RUN_TEST(test_add_node_with_null_key_returns_error);
    RUN_TEST(test_add_node_with_empty_key_returns_error);
    return UNITY_END();
}
