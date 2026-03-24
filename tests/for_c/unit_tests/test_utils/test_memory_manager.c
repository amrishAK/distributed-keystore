
#include "unity.h"
#include "utils/memory_manager.h"
#include <string.h>

// =============================
// Memory Manager Test Utilities
// =============================

// Helper: valid config for pool
static memory_manager_config valid_pool_config() {
    memory_manager_config config = {0};
    config.bucket_size = 8;
    config.sub_bucket_size = 0;
    config.pre_allocation_factor = 0.5;
    config.allocate_list_pool = true;
    config.is_concurrency_enabled = false;
    return config;
}

// Helper: valid config for no pool
static memory_manager_config valid_no_pool_config() {
    memory_manager_config config = {0};
    config.bucket_size = 8;
    config.sub_bucket_size = 0;
    config.pre_allocation_factor = 0.5;
    config.allocate_list_pool = false;
    config.is_concurrency_enabled = false;
    return config;
}


// =============================
// Initialization & Config Tests
// =============================
void test_initialize_memory_manager_invalid_config(void) {
    // Arrange
    memory_manager_config bad1 = valid_pool_config();
    bad1.bucket_size = 0;

    // Act
    int res1 = initialize_memory_manager(bad1);

    // Assert
    TEST_ASSERT_EQUAL_INT(ERR_INVALID_CONFIG, res1);
    cleanup_memory_manager();

    // Arrange
    memory_manager_config bad2 = valid_pool_config();
    bad2.pre_allocation_factor = 0;

    // Act
    int res2 = initialize_memory_manager(bad2);

    // Assert
    TEST_ASSERT_EQUAL_INT(ERR_INVALID_CONFIG, res2);
    cleanup_memory_manager();

    // Arrange
    memory_manager_config bad3 = valid_pool_config();
    bad3.pre_allocation_factor = 2.0;

    // Act
    int res3 = initialize_memory_manager(bad3);

    // Assert
    TEST_ASSERT_EQUAL_INT(ERR_INVALID_CONFIG, res3);
    cleanup_memory_manager();
}


void test_initialize_and_cleanup_memory_manager_pool(void) {
    // Arrange
    memory_manager_config config = valid_pool_config();

    // Act
    int res = initialize_memory_manager(config);

    // Assert
    TEST_ASSERT_EQUAL_INT(0, res);
    TEST_ASSERT_EQUAL_INT(0, cleanup_memory_manager());
}

void test_initialize_and_cleanup_memory_manager_no_pool(void) {
    // Arrange
    memory_manager_config config = valid_no_pool_config();

    // Act
    int res = initialize_memory_manager(config);

    // Assert
    TEST_ASSERT_EQUAL_INT(0, res);
    TEST_ASSERT_EQUAL_INT(0, cleanup_memory_manager());
}


// =============================
// Allocation & Free Tests
// =============================
void test_allocate_and_free_memory_from_pool(void) {
    // Arrange
    memory_manager_config config = valid_pool_config();
    TEST_ASSERT_EQUAL_INT(0, initialize_memory_manager(config));

    // Act
    void *ptr = allocate_memory_from_pool();

    // Assert
    TEST_ASSERT_NOT_NULL(ptr);
    free_memory(ptr, true);
    cleanup_memory_manager();
}

void test_allocate_and_free_memory_no_pool(void) {
    // Arrange
    memory_manager_config config = valid_no_pool_config();
    TEST_ASSERT_EQUAL_INT(0, initialize_memory_manager(config));

    // Act
    void *ptr = allocate_memory_from_pool();

    // Assert
    TEST_ASSERT_NOT_NULL(ptr);
    free_memory(ptr, false);
    cleanup_memory_manager();
}


void test_allocate_memory_and_free(void) {
    // Arrange
    TEST_ASSERT_EQUAL_INT(0, initialize_memory_manager(valid_no_pool_config()));

    // Act
    void *ptr = allocate_memory(32);

    // Assert
    TEST_ASSERT_NOT_NULL(ptr);
    free_memory(ptr, false);
    cleanup_memory_manager();
}

void test_callocate_memory_and_free(void) {
    // Arrange
    TEST_ASSERT_EQUAL_INT(0, initialize_memory_manager(valid_no_pool_config()));

    // Act
    void *ptr = callocate_memory(4, 8);

    // Assert
    TEST_ASSERT_NOT_NULL(ptr);
    unsigned char *c = (unsigned char*)ptr;
    for (size_t i = 0; i < 32; ++i) TEST_ASSERT_EQUAL_UINT8(0, c[i]);
    free_memory(ptr, false);
    cleanup_memory_manager();
}


// =============================
// Reallocation Tests
// =============================
void test_reallocate_memory_grow_and_shrink(void) {
    // Arrange
    TEST_ASSERT_EQUAL_INT(0, initialize_memory_manager(valid_no_pool_config()));

    // Act
    void *ptr = allocate_memory(16);
    TEST_ASSERT_NOT_NULL(ptr);
    memset(ptr, 0xAB, 16);
    void *ptr2 = reallocate_memory(ptr, 32);
    TEST_ASSERT_NOT_NULL(ptr2);
    void *ptr3 = reallocate_memory(ptr2, 8);

    // Assert
    TEST_ASSERT_NOT_NULL(ptr3);
    free_memory(ptr3, false);
    cleanup_memory_manager();
}

void test_reallocate_memory_nullptr_and_zero(void) {
    // Arrange
    TEST_ASSERT_EQUAL_INT(0, initialize_memory_manager(valid_no_pool_config()));

    // Act
    void *ptr = reallocate_memory(NULL, 24);
    void *ptr2 = reallocate_memory(ptr, 0);

    // Assert
    TEST_ASSERT_NOT_NULL(ptr);
    TEST_ASSERT_NULL(ptr2);
    cleanup_memory_manager();
}



// =============================
// Test Suite Entrypoint
// =============================
int test_memory_manager_main(void) {
    UNITY_BEGIN();
    printf("Running Memory Manager Unit Tests...\n");

    // Initialization & Config
    RUN_TEST(test_initialize_memory_manager_invalid_config);
    RUN_TEST(test_initialize_and_cleanup_memory_manager_pool);
    RUN_TEST(test_initialize_and_cleanup_memory_manager_no_pool);

    // Allocation & Free
    RUN_TEST(test_allocate_and_free_memory_from_pool);
    RUN_TEST(test_allocate_and_free_memory_no_pool);
    RUN_TEST(test_allocate_memory_and_free);
    RUN_TEST(test_callocate_memory_and_free);

    // Reallocation
    RUN_TEST(test_reallocate_memory_grow_and_shrink);
    RUN_TEST(test_reallocate_memory_nullptr_and_zero);

    printf("Memory Manager Unit Tests Completed.\n");
    return UNITY_END();
}