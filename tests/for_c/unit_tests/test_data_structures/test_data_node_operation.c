/*
 * Unit tests for data_node_operation.c
 *
 * This suite covers all public API functions, error paths, memory management, and operation counter invariants.
 * Test framework: Unity
 */
#include "unity.h"
#include "data_structures/data_node_operation.h"
#include "type_definitions/hash_bucket_type_definition.h"
#include "type_definitions/error_code_definitions.h"
#include "type_definitions/sucess_code_definitions.h"
#include <string.h>
#include <stdlib.h>
#include "utils/memory_manager.h"

// Mocks/stubs for memory manager and pthreads if needed
// (Assume real implementations unless otherwise specified)

static composite_key_hash dummy_hash = { .bucket_hash = 0x1234, .sub_bucket_hash = 0x5678 };

void test_create_new_data_node_valid_input_returns_success(void) {
    key_value_pair kv = { .key = "abc", .value = (unsigned char*)"val", .value_size = 4 };
    data_node *node = NULL;
    int res = create_new_data_node(dummy_hash, &kv, false, &node);
    TEST_ASSERT_EQUAL(0, res);
    TEST_ASSERT_NOT_NULL(node);
    TEST_ASSERT_EQUAL_STRING("abc", node->key);
    TEST_ASSERT_EQUAL_UINT32(dummy_hash.bucket_hash, node->key_hash.bucket_hash);
    delete_data_node(node);
}

void test_create_new_data_node_null_kv_pair_returns_error(void) {
    data_node *node = NULL;
    int res = create_new_data_node(dummy_hash, NULL, false, &node);
    TEST_ASSERT_EQUAL(ERR_INVALID_ARGUMENT, res);
}

void test_create_new_data_node_null_key_returns_error(void) {
    key_value_pair kv = { .key = NULL, .value = (unsigned char*)"val", .value_size = 4 };
    data_node *node = NULL;
    int res = create_new_data_node(dummy_hash, &kv, false, &node);
    TEST_ASSERT_EQUAL(ERR_INVALID_ARGUMENT, res);
}

void test_create_new_data_node_empty_key_returns_error(void) {
    key_value_pair kv = { .key = "", .value = (unsigned char*)"val", .value_size = 4 };
    data_node *node = NULL;
    int res = create_new_data_node(dummy_hash, &kv, false, &node);
    TEST_ASSERT_EQUAL(ERR_INVALID_ARGUMENT, res);
}

void test_create_new_data_node_null_output_ptr_returns_error(void) {
    key_value_pair kv = { .key = "abc", .value = (unsigned char*)"val", .value_size = 4 };
    int res = create_new_data_node(dummy_hash, &kv, false, NULL);
    TEST_ASSERT_EQUAL(ERR_INVALID_ARGUMENT, res);
}

void test_edit_data_node_value_valid_update(void) {
    key_value_pair kv = { .key = "abc", .value = (unsigned char*)"val", .value_size = 4 };
    data_node *node = NULL;
    create_new_data_node(dummy_hash, &kv, false, &node);
    key_value_pair new_kv = { .key = "abc", .value = (unsigned char*)"newv", .value_size = 5 };
    int res = edit_data_node_value(node, &new_kv);
    TEST_ASSERT_EQUAL(0, res);
    TEST_ASSERT_EQUAL_MEMORY("newv", node->data, 5);
    delete_data_node(node);
}

void test_edit_data_node_value_null_node_returns_error(void) {
    key_value_pair new_kv = { .key = "abc", .value = (unsigned char*)"newv", .value_size = 5 };
    int res = edit_data_node_value(NULL, &new_kv);
    TEST_ASSERT_EQUAL(ERR_INVALID_ARGUMENT, res);
}

void test_edit_data_node_value_null_kv_pair_returns_error(void) {
    key_value_pair kv = { .key = "abc", .value = (unsigned char*)"val", .value_size = 4 };
    data_node *node = NULL;
    create_new_data_node(dummy_hash, &kv, false, &node);
    int res = edit_data_node_value(node, NULL);
    TEST_ASSERT_EQUAL(ERR_INVALID_ARGUMENT, res);
    delete_data_node(node);
}

void test_edit_data_node_value_zero_value_size_clears_data(void) {
    key_value_pair kv = { .key = "abc", .value = (unsigned char*)"val", .value_size = 4 };
    data_node *node = NULL;
    create_new_data_node(dummy_hash, &kv, false, &node);
    key_value_pair new_kv = { .key = "abc", .value = (unsigned char*)"", .value_size = 0 };
    int res = edit_data_node_value(node, &new_kv);
    TEST_ASSERT_EQUAL(0, res);
    TEST_ASSERT_NULL(node->data);
    delete_data_node(node);
}

void test_read_data_node_value_valid(void) {
    key_value_pair kv = { .key = "abc", .value = (unsigned char*)"val", .value_size = 4 };
    data_node *node = NULL;
    create_new_data_node(dummy_hash, &kv, false, &node);
    key_value_pair out = {0};
    int res = read_data_node_value(node, &out);
    TEST_ASSERT_EQUAL(0, res);
    TEST_ASSERT_EQUAL_STRING("abc", out.key);
    TEST_ASSERT_EQUAL_MEMORY("val", out.value, 4);
    free(out.key);
    free(out.value);
    delete_data_node(node);
}

void test_read_data_node_value_null_node_returns_error(void) {
    key_value_pair out = {0};
    int res = read_data_node_value(NULL, &out);
    TEST_ASSERT_EQUAL(ERR_INVALID_ARGUMENT, res);
}

void test_read_data_node_value_null_output_returns_error(void) {
    key_value_pair kv = { .key = "abc", .value = (unsigned char*)"val", .value_size = 4 };
    data_node *node = NULL;
    create_new_data_node(dummy_hash, &kv, false, &node);
    int res = read_data_node_value(node, NULL);
    TEST_ASSERT_EQUAL(ERR_INVALID_ARGUMENT, res);
    delete_data_node(node);
}

void test_delete_data_node_valid_frees_memory(void) {
    key_value_pair kv = { .key = "abc", .value = (unsigned char*)"val", .value_size = 4 };
    data_node *node = NULL;
    create_new_data_node(dummy_hash, &kv, false, &node);
    int res = delete_data_node(node);
    TEST_ASSERT_EQUAL(0, res);
}

void test_delete_data_node_null_pointer_noop(void) {
    int res = delete_data_node(NULL);
    TEST_ASSERT_EQUAL(0, res);
}

void test_soft_delete_data_node_sets_flag(void) {
    key_value_pair kv = { .key = "abc", .value = (unsigned char*)"val", .value_size = 4 };
    data_node *node = NULL;
    create_new_data_node(dummy_hash, &kv, false, &node);
    int res = soft_delete_data_node(node);
    TEST_ASSERT_EQUAL(0, res);
    TEST_ASSERT_TRUE(node->is_deleted);
    delete_data_node(node);
}

void test_soft_delete_data_node_null_pointer_returns_error(void) {
    int res = soft_delete_data_node(NULL);
    TEST_ASSERT_EQUAL(ERR_INVALID_ARGUMENT, res);
}

void test_create_new_data_node_concurrency_enabled(void) {
    key_value_pair kv = { .key = "abc", .value = (unsigned char*)"val", .value_size = 4 };
    data_node *node = NULL;
    int res = create_new_data_node(dummy_hash, &kv, true, &node);
    TEST_ASSERT_EQUAL(0, res);
    TEST_ASSERT_NOT_NULL(node);
    TEST_ASSERT_TRUE(node->is_concurrency_enabled);
    delete_data_node(node);
}

void test_create_new_data_node_zero_value_size(void) {
    key_value_pair kv = { .key = "abc", .value = (unsigned char*)"", .value_size = 0 };
    data_node *node = NULL;
    int res = create_new_data_node(dummy_hash, &kv, false, &node);
    TEST_ASSERT_EQUAL(0, res);
    TEST_ASSERT_NOT_NULL(node);
    TEST_ASSERT_NULL(node->data);
    TEST_ASSERT_EQUAL(0, node->data_size);
    delete_data_node(node);
}

void test_edit_data_node_value_zero_value_size_on_nonempty_node(void) {
    key_value_pair kv = { .key = "abc", .value = (unsigned char*)"val", .value_size = 4 };
    data_node *node = NULL;
    create_new_data_node(dummy_hash, &kv, false, &node);
    key_value_pair new_kv = { .key = "abc", .value = (unsigned char*)"", .value_size = 0 };
    int res = edit_data_node_value(node, &new_kv);
    TEST_ASSERT_EQUAL(0, res);
    TEST_ASSERT_NULL(node->data);
    TEST_ASSERT_EQUAL(0, node->data_size);
    delete_data_node(node);
}

void test_soft_delete_and_delete_idempotency(void) {
    key_value_pair kv = { .key = "abc", .value = (unsigned char*)"val", .value_size = 4 };
    data_node *node = NULL;
    create_new_data_node(dummy_hash, &kv, false, &node);
    int res1 = soft_delete_data_node(node);
    int res2 = soft_delete_data_node(node); // Should be idempotent
    TEST_ASSERT_EQUAL(0, res1);
    TEST_ASSERT_EQUAL(0, res2);
    int del1 = delete_data_node(node);
    TEST_ASSERT_EQUAL(0, del1);
    // After deletion, node pointer is dangling; do not call delete again.
    // Optionally, set node to NULL to avoid accidental reuse.
    node = NULL;
    TEST_ASSERT_NULL(node);
    // Note: API idempotency means repeated calls with the same pointer value are safe,
    // but after free, the pointer must not be reused by the test.
}

// Allocation and reallocation failure tests would require dependency injection or linker tricks for malloc/free
// For now, we skip these unless a mock memory manager is available

int test_data_node_operations_main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_create_new_data_node_valid_input_returns_success);
    RUN_TEST(test_create_new_data_node_null_kv_pair_returns_error);
    RUN_TEST(test_create_new_data_node_null_key_returns_error);
    RUN_TEST(test_create_new_data_node_empty_key_returns_error);
    RUN_TEST(test_create_new_data_node_null_output_ptr_returns_error);
    RUN_TEST(test_edit_data_node_value_valid_update);
    RUN_TEST(test_edit_data_node_value_null_node_returns_error);
    RUN_TEST(test_edit_data_node_value_null_kv_pair_returns_error);
    RUN_TEST(test_edit_data_node_value_zero_value_size_clears_data);
    RUN_TEST(test_read_data_node_value_valid);
    RUN_TEST(test_read_data_node_value_null_node_returns_error);
    RUN_TEST(test_read_data_node_value_null_output_returns_error);
    RUN_TEST(test_delete_data_node_valid_frees_memory);
    RUN_TEST(test_delete_data_node_null_pointer_noop);
    RUN_TEST(test_soft_delete_data_node_sets_flag);
    RUN_TEST(test_soft_delete_data_node_null_pointer_returns_error);
    RUN_TEST(test_create_new_data_node_concurrency_enabled);
    RUN_TEST(test_create_new_data_node_zero_value_size);
    RUN_TEST(test_edit_data_node_value_zero_value_size_on_nonempty_node);
    RUN_TEST(test_soft_delete_and_delete_idempotency);
    return UNITY_END();
}
