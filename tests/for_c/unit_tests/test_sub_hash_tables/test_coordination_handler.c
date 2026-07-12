#include "unity.h"
#include "sub_hash_table/operation_handlers/coordination_handler.h"
#include "sub_hash_table/operation_handlers/data_handler.h"
#include "sub_hash_table/sub_hash_bucket_operation.h"
#include "data_structures/data_node_operation.h"
#include "type_definitions/error_code_definitions.h"
#include "type_definitions/sucess_code_definitions.h"

#include <string.h>

static data_node* create_node(const char* key, const char* value, composite_key_hash hash, bool is_concurrency_enabled) {
    data_node* node = NULL;
    int result = create_data_node_from_value(hash, key, (const unsigned char*)value, strlen(value) + 1, is_concurrency_enabled, &node);
    TEST_ASSERT_EQUAL_INT(SUCCESS, result);
    TEST_ASSERT_NOT_NULL(node);
    return node;
}

void test_add_data_node_and_find_data_node_returns_success(void) {
    sub_hash_bucket bucket = {0};
    composite_key_hash hash = {33, 44};

    TEST_ASSERT_EQUAL_INT(SUCCESS, initialise_sub_hash_bucket(&bucket, false, 16));

    sub_hash_bucket_operation_args args = {&bucket, "key", hash};
    data_node* node = create_node("key", "value", hash, false);

    TEST_ASSERT_EQUAL_INT(SUCCESS, add_data_node(args, node));

    data_node* found = NULL;
    TEST_ASSERT_EQUAL_INT(SUCCESS, find_data_node(args, &found));
    TEST_ASSERT_EQUAL_PTR(node, found);

    cleanup_sub_hash_bucket(&bucket);
}

void test_find_data_node_missing_returns_not_found(void) {
    sub_hash_bucket bucket = {0};

    TEST_ASSERT_EQUAL_INT(SUCCESS, initialise_sub_hash_bucket(&bucket, false, 16));

    sub_hash_bucket_operation_args args = {&bucket, "missing", {55, 66}};
    data_node* found = NULL;

    int result = find_data_node(args, &found);
    TEST_ASSERT_EQUAL_INT(ERR_DATA_NODE_NOT_FOUND, result);
    TEST_ASSERT_NULL(found);

    cleanup_sub_hash_bucket(&bucket);
}

void test_delete_all_data_nodes_clears_list_and_resets_bloom_filter(void) {
    sub_hash_bucket bucket = {0};
    composite_key_hash hash = {77, 88};

    TEST_ASSERT_EQUAL_INT(SUCCESS, initialise_sub_hash_bucket(&bucket, false, 16));

    sub_hash_bucket_operation_args args = {&bucket, "key", hash};
    data_node* node = create_node("key", "value", hash, false);

    TEST_ASSERT_EQUAL_INT(SUCCESS, add_data_node(args, node));
    TEST_ASSERT_NOT_NULL(bucket.linked_list_head);

    sub_hash_bucket_operation_args delete_args = {&bucket, NULL, {0, 0}};
    TEST_ASSERT_EQUAL_INT(SUCCESS, delete_all_data_nodes(delete_args));
    TEST_ASSERT_NULL(bucket.linked_list_head);

    cleanup_sub_hash_bucket(&bucket);
}

void test_cleanup_deleted_data_nodes_returns_deleted_count(void) {
    sub_hash_bucket bucket = {0};
    composite_key_hash deleted_hash = {100, 101};
    composite_key_hash active_hash = {100, 202};

    TEST_ASSERT_EQUAL_INT(SUCCESS, initialise_sub_hash_bucket(&bucket, false, 16));

    sub_hash_bucket_operation_args deleted_args = {&bucket, "deleted", deleted_hash};
    sub_hash_bucket_operation_args active_args = {&bucket, "active", active_hash};

    data_node* deleted_node = create_node("deleted", "value-a", deleted_hash, false);
    data_node* active_node = create_node("active", "value-b", active_hash, false);

    TEST_ASSERT_EQUAL_INT(SUCCESS, add_data_node(deleted_args, deleted_node));
    TEST_ASSERT_EQUAL_INT(SUCCESS, add_data_node(active_args, active_node));

    TEST_ASSERT_EQUAL_INT(SUCCESS, soft_delete_data_node(deleted_node));

    unsigned int deleted_count = 0;
    sub_hash_bucket_operation_args cleanup_args = {&bucket, NULL, {0, 0}};
    int cleanup_result = cleanup_deleted_data_nodes(cleanup_args, &deleted_count);

    TEST_ASSERT_EQUAL_INT(1, cleanup_result);
    TEST_ASSERT_EQUAL_UINT(1, deleted_count);

    data_node* found_deleted = NULL;
    data_node* found_active = NULL;
    TEST_ASSERT_EQUAL_INT(ERR_DATA_NODE_NOT_FOUND, find_data_node(deleted_args, &found_deleted));
    TEST_ASSERT_EQUAL_INT(SUCCESS, find_data_node(active_args, &found_active));
    TEST_ASSERT_EQUAL_PTR(active_node, found_active);

    cleanup_sub_hash_bucket(&bucket);
}

void test_coordination_handler_with_concurrency_enabled_add_and_find(void) {
    sub_hash_bucket bucket = {0};
    composite_key_hash hash = {400, 500};

    TEST_ASSERT_EQUAL_INT(SUCCESS, initialise_sub_hash_bucket(&bucket, true, 16));

    sub_hash_bucket_operation_args args = {&bucket, "key", hash};
    data_node* node = create_node("key", "value", hash, true);

    TEST_ASSERT_EQUAL_INT(SUCCESS, add_data_node(args, node));

    data_node* found = NULL;
    TEST_ASSERT_EQUAL_INT(SUCCESS, find_data_node(args, &found));
    TEST_ASSERT_EQUAL_PTR(node, found);

    cleanup_sub_hash_bucket(&bucket);
}

int test_coordination_handler_main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_add_data_node_and_find_data_node_returns_success);
    RUN_TEST(test_find_data_node_missing_returns_not_found);
    RUN_TEST(test_delete_all_data_nodes_clears_list_and_resets_bloom_filter);
    RUN_TEST(test_cleanup_deleted_data_nodes_returns_deleted_count);
    RUN_TEST(test_coordination_handler_with_concurrency_enabled_add_and_find);
    return UNITY_END();
}
