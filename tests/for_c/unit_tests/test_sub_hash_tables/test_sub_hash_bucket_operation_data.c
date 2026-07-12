#include "unity.h"
#include "sub_hash_table/sub_hash_bucket_operation.h"
#include "type_definitions/sucess_code_definitions.h"

#include <string.h>
#include <stdlib.h>

void test_add_and_get_node_from_sub_hash_bucket(void) {
    sub_hash_bucket bucket = {0};
    initialise_sub_hash_bucket(&bucket, false, 4);

    unsigned char value[] = "value";
    key_value_pair kv = {"key", value, strlen((char*)value) + 1};
    sub_hash_bucket_operation_args args = {&bucket, "key", {123, 0}};
    key_value_pair out = {0};

    int add_result = add_node_to_sub_hash_bucket(args, &kv);
    int get_result = get_key_store_value_from_sub_hash_bucket(args, &out);

    TEST_ASSERT_EQUAL_INT(SUCESS_ADDED_NEW_NODE, add_result);
    TEST_ASSERT_EQUAL_UINT(1, bucket.active_node_count);
    TEST_ASSERT_EQUAL_INT(SUCCESS, get_result);
    TEST_ASSERT_EQUAL_STRING("key", out.key);
    TEST_ASSERT_EQUAL_STRING("value", (char*)out.value);

    free(out.key);
    free(out.value);
    cleanup_sub_hash_bucket(&bucket);
}

void test_update_node_in_sub_hash_bucket(void) {
    sub_hash_bucket bucket = {0};
    initialise_sub_hash_bucket(&bucket, false, 4);

    unsigned char value[] = "value";
    key_value_pair kv = {"key", value, strlen((char*)value) + 1};
    sub_hash_bucket_operation_args args = {&bucket, "key", {123, 0}};
    add_node_to_sub_hash_bucket(args, &kv);

    unsigned char new_value[] = "newval";
    key_value_pair new_kv = {"key", new_value, strlen((char*)new_value) + 1};
    key_value_pair out = {0};

    int update_result = update_node_in_sub_hash_bucket(args, &new_kv);
    int get_result = get_key_store_value_from_sub_hash_bucket(args, &out);

    TEST_ASSERT_EQUAL_INT(SUCCESS, update_result);
    TEST_ASSERT_EQUAL_UINT(1, bucket.active_node_count);
    TEST_ASSERT_EQUAL_INT(SUCCESS, get_result);
    TEST_ASSERT_EQUAL_STRING("newval", (char*)out.value);

    free(out.key);
    free(out.value);
    cleanup_sub_hash_bucket(&bucket);
}

void test_delete_key_from_sub_hash_bucket(void) {
    sub_hash_bucket bucket = {0};
    initialise_sub_hash_bucket(&bucket, false, 4);

    unsigned char value[] = "value";
    key_value_pair kv = {"key", value, strlen((char*)value) + 1};
    sub_hash_bucket_operation_args args = {&bucket, "key", {123, 0}};
    add_node_to_sub_hash_bucket(args, &kv);

    key_value_pair out = {0};
    int delete_result = delete_key_from_sub_hash_bucket(args);
    int get_result = get_key_store_value_from_sub_hash_bucket(args, &out);

    TEST_ASSERT_EQUAL_INT(SUCCESS, delete_result);
    TEST_ASSERT_EQUAL_UINT(0, bucket.active_node_count);
    TEST_ASSERT_LESS_THAN_INT(0, get_result);

    if (out.key != NULL) free(out.key);
    if (out.value != NULL) free(out.value);
    cleanup_sub_hash_bucket(&bucket);
}

void test_delete_nonexistent_key_from_sub_hash_bucket_returns_error(void) {
    sub_hash_bucket bucket = {0};
    initialise_sub_hash_bucket(&bucket, false, 4);

    sub_hash_bucket_operation_args args = {&bucket, "nope", {999, 0}};
    int delete_result = delete_key_from_sub_hash_bucket(args);

    TEST_ASSERT_LESS_THAN_INT(0, delete_result);
    cleanup_sub_hash_bucket(&bucket);
}

void test_multiple_nodes_chain_sub_hash_bucket(void) {
    sub_hash_bucket bucket = {0};
    initialise_sub_hash_bucket(&bucket, false, 4);

    unsigned char value_a[] = "v1";
    unsigned char value_b[] = "v2";
    key_value_pair kv_a = {"A", value_a, strlen((char*)value_a) + 1};
    key_value_pair kv_b = {"B", value_b, strlen((char*)value_b) + 1};

    uint64_t fake_sub_hash = 12345;
    sub_hash_bucket_operation_args args_a = {&bucket, "A", {1, fake_sub_hash}};
    sub_hash_bucket_operation_args args_b = {&bucket, "B", {2, fake_sub_hash}};

    key_value_pair out_a = {0};
    key_value_pair out_b = {0};

    TEST_ASSERT_EQUAL_INT(SUCESS_ADDED_NEW_NODE, add_node_to_sub_hash_bucket(args_a, &kv_a));
    TEST_ASSERT_EQUAL_INT(SUCESS_ADDED_NEW_NODE, add_node_to_sub_hash_bucket(args_b, &kv_b));

    TEST_ASSERT_EQUAL_INT(SUCCESS, get_key_store_value_from_sub_hash_bucket(args_a, &out_a));
    TEST_ASSERT_EQUAL_INT(SUCCESS, get_key_store_value_from_sub_hash_bucket(args_b, &out_b));

    TEST_ASSERT_EQUAL_UINT(2, bucket.active_node_count);
    TEST_ASSERT_EQUAL_STRING("A", out_a.key);
    TEST_ASSERT_EQUAL_STRING("B", out_b.key);

    free(out_a.key);
    free(out_a.value);
    free(out_b.key);
    free(out_b.value);
    cleanup_sub_hash_bucket(&bucket);
}

void test_update_delete_get_after_sub_hash_bucket_cleanup(void) {
    sub_hash_bucket bucket = {0};
    initialise_sub_hash_bucket(&bucket, false, 4);

    unsigned char value[] = "v";
    key_value_pair kv = {"gone", value, strlen((char*)value) + 1};
    sub_hash_bucket_operation_args args = {&bucket, "gone", {77, 0}};
    key_value_pair out = {0};

    add_node_to_sub_hash_bucket(args, &kv);
    cleanup_sub_hash_bucket(&bucket);

    TEST_ASSERT_LESS_THAN_INT(0, update_node_in_sub_hash_bucket(args, &kv));
    TEST_ASSERT_LESS_THAN_INT(0, delete_key_from_sub_hash_bucket(args));
    TEST_ASSERT_LESS_THAN_INT(0, get_key_store_value_from_sub_hash_bucket(args, &out));

    if (out.key != NULL) free(out.key);
    if (out.value != NULL) free(out.value);
}

void test_repeated_add_update_delete_same_key_sub_hash_bucket(void) {
    sub_hash_bucket bucket = {0};
    initialise_sub_hash_bucket(&bucket, false, 4);

    unsigned char value_1[] = "v1";
    unsigned char value_2[] = "v2";
    key_value_pair kv = {"repeat", value_1, strlen((char*)value_1) + 1};
    sub_hash_bucket_operation_args args = {&bucket, "repeat", {55, 0}};

    TEST_ASSERT_EQUAL_INT(SUCESS_ADDED_NEW_NODE, add_node_to_sub_hash_bucket(args, &kv));

    kv.value = value_2;
    kv.value_size = strlen((char*)value_2) + 1;

    TEST_ASSERT_EQUAL_INT(SUCCESS, update_node_in_sub_hash_bucket(args, &kv));
    TEST_ASSERT_EQUAL_INT(SUCCESS, delete_key_from_sub_hash_bucket(args));
    TEST_ASSERT_LESS_THAN_INT(0, delete_key_from_sub_hash_bucket(args));
    TEST_ASSERT_EQUAL_INT(SUCESS_ADDED_NEW_NODE, add_node_to_sub_hash_bucket(args, &kv));

    cleanup_sub_hash_bucket(&bucket);
}

void test_edit_sub_hash_bucket_node_after_deletion_returns_error(void) {
    sub_hash_bucket bucket = {0};
    initialise_sub_hash_bucket(&bucket, false, 4);

    unsigned char value[] = "value";
    key_value_pair kv = {"key", value, strlen((char*)value) + 1};
    sub_hash_bucket_operation_args args = {&bucket, "key", {55, 0}};

    TEST_ASSERT_EQUAL_INT(SUCESS_ADDED_NEW_NODE, add_node_to_sub_hash_bucket(args, &kv));
    TEST_ASSERT_EQUAL_INT(SUCCESS, delete_key_from_sub_hash_bucket(args));

    unsigned char new_value[] = "newval";
    key_value_pair new_kv = {"key", new_value, strlen((char*)new_value) + 1};

    TEST_ASSERT_LESS_THAN_INT(0, update_node_in_sub_hash_bucket(args, &new_kv));

    cleanup_sub_hash_bucket(&bucket);
}

void test_read_sub_hash_bucket_node_after_deletion_returns_error(void) {
    sub_hash_bucket bucket = {0};
    initialise_sub_hash_bucket(&bucket, false, 4);

    unsigned char value[] = "value";
    key_value_pair kv = {"key", value, strlen((char*)value) + 1};
    sub_hash_bucket_operation_args args = {&bucket, "key", {55, 0}};

    TEST_ASSERT_EQUAL_INT(SUCESS_ADDED_NEW_NODE, add_node_to_sub_hash_bucket(args, &kv));
    TEST_ASSERT_EQUAL_INT(SUCCESS, delete_key_from_sub_hash_bucket(args));

    key_value_pair out = {0};
    int get_result = get_key_store_value_from_sub_hash_bucket(args, &out);

    TEST_ASSERT_LESS_THAN_INT(0, get_result);
    if (out.key != NULL) free(out.key);
    if (out.value != NULL) free(out.value);

    cleanup_sub_hash_bucket(&bucket);
}

void test_delete_sub_hash_bucket_node_twice_returns_error_second_time(void) {
    sub_hash_bucket bucket = {0};
    initialise_sub_hash_bucket(&bucket, false, 4);

    unsigned char value[] = "value";
    key_value_pair kv = {"key", value, strlen((char*)value) + 1};
    sub_hash_bucket_operation_args args = {&bucket, "key", {55, 0}};

    TEST_ASSERT_EQUAL_INT(SUCESS_ADDED_NEW_NODE, add_node_to_sub_hash_bucket(args, &kv));
    TEST_ASSERT_EQUAL_INT(SUCCESS, delete_key_from_sub_hash_bucket(args));
    TEST_ASSERT_LESS_THAN_INT(0, delete_key_from_sub_hash_bucket(args));

    cleanup_sub_hash_bucket(&bucket);
}

void test_add_nodes_until_triggering_resize(void) {
    sub_hash_bucket bucket = {0};
    initialise_sub_hash_bucket(&bucket, false, 2);

    unsigned char value_a[] = "v1";
    unsigned char value_b[] = "v2";
    unsigned char value_c[] = "v3";

    key_value_pair kv_a = {"a", value_a, strlen((char*)value_a) + 1};
    key_value_pair kv_b = {"b", value_b, strlen((char*)value_b) + 1};
    key_value_pair kv_c = {"c", value_c, strlen((char*)value_c) + 1};

    sub_hash_bucket_operation_args args_a = {&bucket, "a", {1, 11}};
    sub_hash_bucket_operation_args args_b = {&bucket, "b", {2, 22}};
    sub_hash_bucket_operation_args args_c = {&bucket, "c", {3, 33}};

    TEST_ASSERT_EQUAL_INT(SUCESS_ADDED_NEW_NODE, add_node_to_sub_hash_bucket(args_a, &kv_a));
    TEST_ASSERT_EQUAL_INT(SUCESS_ADDED_NEW_NODE, add_node_to_sub_hash_bucket(args_b, &kv_b));

    int resize_result = add_node_to_sub_hash_bucket(args_c, &kv_c);
    TEST_ASSERT_EQUAL_INT(SUCESS_ADDED_NEW_NODE_RESZING_TRIGGERED, resize_result);
    TEST_ASSERT_EQUAL_UINT(3, bucket.active_node_count);

    cleanup_sub_hash_bucket(&bucket);
}

int test_sub_hash_bucket_operation_data_main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_add_and_get_node_from_sub_hash_bucket);
    RUN_TEST(test_update_node_in_sub_hash_bucket);
    RUN_TEST(test_delete_key_from_sub_hash_bucket);
    RUN_TEST(test_delete_nonexistent_key_from_sub_hash_bucket_returns_error);
    RUN_TEST(test_multiple_nodes_chain_sub_hash_bucket);
    RUN_TEST(test_update_delete_get_after_sub_hash_bucket_cleanup);
    RUN_TEST(test_repeated_add_update_delete_same_key_sub_hash_bucket);
    RUN_TEST(test_edit_sub_hash_bucket_node_after_deletion_returns_error);
    RUN_TEST(test_read_sub_hash_bucket_node_after_deletion_returns_error);
    RUN_TEST(test_delete_sub_hash_bucket_node_twice_returns_error_second_time);
    RUN_TEST(test_add_nodes_until_triggering_resize);
    return UNITY_END();
}
