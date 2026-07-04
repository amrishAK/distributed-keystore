#include "unity.h"
#include "sub_hash_table/operation_handlers/data_handler.h"
#include "data_structures/data_node_operation.h"
#include "type_definitions/error_code_definitions.h"
#include "type_definitions/sucess_code_definitions.h"

#include <string.h>
#include <stdlib.h>

static const composite_key_hash TEST_HASH = {101, 202};

void test_create_data_node_from_value_valid_input_returns_success(void) {
    unsigned char value[] = "value";
    data_node* new_node = NULL;

    int result = create_data_node_from_value(TEST_HASH, "key", value, strlen((char*)value) + 1, false, &new_node);

    TEST_ASSERT_EQUAL_INT(SUCCESS, result);
    TEST_ASSERT_NOT_NULL(new_node);
    TEST_ASSERT_EQUAL_STRING("key", new_node->key);
    TEST_ASSERT_EQUAL_UINT64(TEST_HASH.sub_bucket_hash, new_node->key_hash.sub_bucket_hash);

    delete_data_node(new_node);
}

void test_create_data_node_from_value_null_key_returns_error(void) {
    unsigned char value[] = "value";
    data_node* new_node = NULL;

    int result = create_data_node_from_value(TEST_HASH, NULL, value, strlen((char*)value) + 1, false, &new_node);

    TEST_ASSERT_EQUAL_INT(ERR_INVALID_ARGUMENT, result);
    TEST_ASSERT_NULL(new_node);
}

void test_get_data_node_value_without_concurrency_returns_copy(void) {
    unsigned char value[] = "value";
    data_node* node = NULL;
    key_value_pair out = {0};

    TEST_ASSERT_EQUAL_INT(SUCCESS, create_data_node_from_value(TEST_HASH, "key", value, strlen((char*)value) + 1, false, &node));

    int result = get_data_node_value(node, false, &out);

    TEST_ASSERT_EQUAL_INT(SUCCESS, result);
    TEST_ASSERT_EQUAL_STRING("key", out.key);
    TEST_ASSERT_EQUAL_STRING("value", (char*)out.value);

    free(out.key);
    free(out.value);
    delete_data_node(node);
}

void test_get_data_node_value_with_concurrency_returns_copy(void) {
    unsigned char value[] = "value";
    data_node* node = NULL;
    key_value_pair out = {0};

    TEST_ASSERT_EQUAL_INT(SUCCESS, create_data_node_from_value(TEST_HASH, "key", value, strlen((char*)value) + 1, true, &node));

    int result = get_data_node_value(node, true, &out);

    TEST_ASSERT_EQUAL_INT(SUCCESS, result);
    TEST_ASSERT_EQUAL_STRING("key", out.key);
    TEST_ASSERT_EQUAL_STRING("value", (char*)out.value);

    free(out.key);
    free(out.value);
    delete_data_node(node);
}

void test_update_data_node_without_concurrency_updates_value(void) {
    unsigned char value[] = "value";
    unsigned char new_value[] = "new-value";
    data_node* node = NULL;

    TEST_ASSERT_EQUAL_INT(SUCCESS, create_data_node_from_value(TEST_HASH, "key", value, strlen((char*)value) + 1, false, &node));

    key_value_pair update = {"key", new_value, strlen((char*)new_value) + 1};
    int result = update_data_node(node, false, &update);

    TEST_ASSERT_EQUAL_INT(SUCCESS, result);
    TEST_ASSERT_EQUAL_STRING("new-value", (char*)node->data);

    delete_data_node(node);
}

void test_update_data_node_with_concurrency_updates_value(void) {
    unsigned char value[] = "value";
    unsigned char new_value[] = "new-value";
    data_node* node = NULL;

    TEST_ASSERT_EQUAL_INT(SUCCESS, create_data_node_from_value(TEST_HASH, "key", value, strlen((char*)value) + 1, true, &node));

    key_value_pair update = {"key", new_value, strlen((char*)new_value) + 1};
    int result = update_data_node(node, true, &update);

    TEST_ASSERT_EQUAL_INT(SUCCESS, result);
    TEST_ASSERT_EQUAL_STRING("new-value", (char*)node->data);

    delete_data_node(node);
}

void test_update_data_node_null_node_without_concurrency_returns_error(void) {
    unsigned char value[] = "new-value";
    key_value_pair update = {"key", value, strlen((char*)value) + 1};

    int result = update_data_node(NULL, false, &update);

    TEST_ASSERT_EQUAL_INT(ERR_INVALID_ARGUMENT, result);
}

void test_soft_delete_without_concurrency_sets_deleted_flag(void) {
    unsigned char value[] = "value";
    data_node* node = NULL;

    TEST_ASSERT_EQUAL_INT(SUCCESS, create_data_node_from_value(TEST_HASH, "key", value, strlen((char*)value) + 1, false, &node));

    int result = soft_delete(node, false);

    TEST_ASSERT_EQUAL_INT(SUCCESS, result);
    TEST_ASSERT_TRUE(node->is_deleted);

    delete_data_node(node);
}

void test_soft_delete_with_concurrency_sets_deleted_flag(void) {
    unsigned char value[] = "value";
    data_node* node = NULL;

    TEST_ASSERT_EQUAL_INT(SUCCESS, create_data_node_from_value(TEST_HASH, "key", value, strlen((char*)value) + 1, true, &node));

    int result = soft_delete(node, true);

    TEST_ASSERT_EQUAL_INT(SUCCESS, result);
    TEST_ASSERT_TRUE(node->is_deleted);

    delete_data_node(node);
}

int test_data_handler_main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_create_data_node_from_value_valid_input_returns_success);
    RUN_TEST(test_create_data_node_from_value_null_key_returns_error);
    RUN_TEST(test_get_data_node_value_without_concurrency_returns_copy);
    RUN_TEST(test_get_data_node_value_with_concurrency_returns_copy);
    RUN_TEST(test_update_data_node_without_concurrency_updates_value);
    RUN_TEST(test_update_data_node_with_concurrency_updates_value);
    RUN_TEST(test_update_data_node_null_node_without_concurrency_returns_error);
    RUN_TEST(test_soft_delete_without_concurrency_sets_deleted_flag);
    RUN_TEST(test_soft_delete_with_concurrency_sets_deleted_flag);
    return UNITY_END();
}
