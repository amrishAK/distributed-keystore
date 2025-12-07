#include "unity.h"
#include "utils/memory_manager.h"
#include <stdlib.h>
#include <stdbool.h>

void test_initialize_memory_manager_valid_config(void) {
    memory_manager_config config = { .bucket_size = 10, .pre_allocation_factor = 0.5, .allocate_list_pool = true, .allocate_tree_pool = false, .is_concurrency_enabled = false };
    TEST_ASSERT_EQUAL(0, initialize_memory_manager(config));
    TEST_ASSERT_EQUAL(0, cleanup_memory_manager());
}

void test_initialize_memory_manager_invalid_config(void) {
    memory_manager_config config1 = { .bucket_size = 0, .pre_allocation_factor = 0.5, .allocate_list_pool = true, .allocate_tree_pool = false, .is_concurrency_enabled = false };
    TEST_ASSERT_EQUAL(-1, initialize_memory_manager(config1));
    memory_manager_config config2 = { .bucket_size = 10, .pre_allocation_factor = 0.0, .allocate_list_pool = true, .allocate_tree_pool = false, .is_concurrency_enabled = false };
    TEST_ASSERT_EQUAL(-1, initialize_memory_manager(config2));
    memory_manager_config config3 = { .bucket_size = 10, .pre_allocation_factor = 1.5, .allocate_list_pool = true, .allocate_tree_pool = false, .is_concurrency_enabled = false };
    TEST_ASSERT_EQUAL(-1, initialize_memory_manager(config3));
}

void test_allocate_and_free_from_list_pool(void) {
    memory_manager_config config = { .bucket_size = 5, .pre_allocation_factor = 1.0, .allocate_list_pool = true, .allocate_tree_pool = false, .is_concurrency_enabled = false };
    int result = initialize_memory_manager(config);
    TEST_ASSERT_EQUAL_MESSAGE(0, result, "Failed to initialize memory manager");
    void *ptrs[5];
    for(int i = 0; i < 5; ++i) {
        ptrs[i] = allocate_memory_from_pool(LIST_POOL);
        TEST_ASSERT_NOT_NULL_MESSAGE(ptrs[i], "Failed to allocate memory from list pool");
    }

    // Pool exhausted, should fallback to malloc
    void *extra = allocate_memory_from_pool(LIST_POOL);
    TEST_ASSERT_NOT_NULL_MESSAGE(extra, "Failed to allocate extra memory from list pool");
    // Free all pool pointers
    for(int i = 0; i < 5; ++i) {
        free_memory(ptrs[i], LIST_POOL);
    }
    // Free malloc fallback
    free_memory(extra, LIST_POOL);
    result = cleanup_memory_manager();
    TEST_ASSERT_EQUAL_MESSAGE(0, result, "Failed to cleanup memory manager");
}

void test_allocate_and_free_from_tree_pool(void) {
    memory_manager_config config = { .bucket_size = 3, .pre_allocation_factor = 1.0, .allocate_list_pool = false, .allocate_tree_pool = true, .is_concurrency_enabled = false };
    TEST_ASSERT_EQUAL_MESSAGE(0, initialize_memory_manager(config), "Failed to initialize memory manager");
    void *ptrs[3];
    for(int i = 0; i < 3; ++i) {
        ptrs[i] = allocate_memory_from_pool(TREE_POOL);
        TEST_ASSERT_NOT_NULL_MESSAGE(ptrs[i], "Failed to allocate memory from tree pool");
    }
    void *extra = allocate_memory_from_pool(TREE_POOL);
    TEST_ASSERT_NOT_NULL_MESSAGE(extra, "Failed to allocate extra memory from tree pool");
    for(int i = 0; i < 3; ++i) {
        free_memory(ptrs[i], TREE_POOL);
    }
    free_memory(extra, TREE_POOL);
    TEST_ASSERT_EQUAL_MESSAGE(0, cleanup_memory_manager(), "Failed to cleanup memory manager");
}

void test_free_invalid_pointer(void) {
    memory_manager_config config = { .bucket_size = 2, .pre_allocation_factor = 1.0, .allocate_list_pool = true, .allocate_tree_pool = false, .is_concurrency_enabled = false };
    TEST_ASSERT_EQUAL(0, initialize_memory_manager(config));
    int *invalid_ptr = malloc(sizeof(int));
    // Should fallback to standard free
    free_memory(invalid_ptr, LIST_POOL);
    TEST_ASSERT_EQUAL(0, cleanup_memory_manager());
}

void test_double_free(void) {
    memory_manager_config config = { .bucket_size = 2, .pre_allocation_factor = 1.0, .allocate_list_pool = true, .allocate_tree_pool = false, .is_concurrency_enabled = false };
    TEST_ASSERT_EQUAL(0, initialize_memory_manager(config));
    void *ptr = allocate_memory_from_pool(LIST_POOL);
    TEST_ASSERT_NOT_NULL(ptr);
    free_memory(ptr, LIST_POOL);
    // Double free: should fallback to standard free or handle gracefully
    free_memory(ptr, LIST_POOL);
    TEST_ASSERT_EQUAL(0, cleanup_memory_manager());
}

void test_allocate_memory_heap(void) {
    void *ptr = allocate_memory(128);
    TEST_ASSERT_NOT_NULL(ptr);
    free_memory(ptr, NO_POOL);
}

void test_initialize_memory_manager_both_pools(void) {
    memory_manager_config config = { .bucket_size = 4, .pre_allocation_factor = 1.0, .allocate_list_pool = true, .allocate_tree_pool = true, .is_concurrency_enabled = false };
    TEST_ASSERT_EQUAL(0, initialize_memory_manager(config));
    void *list_ptr = allocate_memory_from_pool(LIST_POOL);
    void *tree_ptr = allocate_memory_from_pool(TREE_POOL);
    TEST_ASSERT_NOT_NULL(list_ptr);
    TEST_ASSERT_NOT_NULL(tree_ptr);
    free_memory(list_ptr, LIST_POOL);
    free_memory(tree_ptr, TREE_POOL);
    TEST_ASSERT_EQUAL(0, cleanup_memory_manager());
}

void test_cleanup_without_initialization(void) {
    // Should handle cleanup gracefully even if not initialized
    TEST_ASSERT_EQUAL(0, cleanup_memory_manager());
}

void test_allocate_memory_from_uninitialized_pool(void) {
    // Try to allocate from pool before initialization
    void *ptr = allocate_memory_from_pool(LIST_POOL);
    TEST_ASSERT_NULL(ptr);
}

void test_free_null_pointer(void) {
    memory_manager_config config = { .bucket_size = 2, .pre_allocation_factor = 1.0, .allocate_list_pool = true, .allocate_tree_pool = false, .is_concurrency_enabled = false };
    TEST_ASSERT_EQUAL(0, initialize_memory_manager(config));
    // Should handle NULL pointer gracefully
    free_memory(NULL, LIST_POOL);
    TEST_ASSERT_EQUAL(0, cleanup_memory_manager());
}

int test_memory_manager_suite(void) {
    printf("Running Memory Manager Tests...\n");
    RUN_TEST(test_initialize_memory_manager_valid_config);
    RUN_TEST(test_initialize_memory_manager_invalid_config);
    RUN_TEST(test_allocate_and_free_from_list_pool);
    RUN_TEST(test_allocate_and_free_from_tree_pool);
    RUN_TEST(test_free_invalid_pointer);
    RUN_TEST(test_double_free);
    RUN_TEST(test_allocate_memory_heap);
    RUN_TEST(test_initialize_memory_manager_both_pools);
    RUN_TEST(test_cleanup_without_initialization);
    RUN_TEST(test_allocate_memory_from_uninitialized_pool);
    RUN_TEST(test_free_null_pointer);
    printf("Memory manager tests completed.\n");
    return 0;
}
