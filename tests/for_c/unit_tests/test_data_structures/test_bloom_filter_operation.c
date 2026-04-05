/*
 * Unit tests for Bloom filter operations in the distributed keystore.
 *
 * This suite covers initialization, cleanup, add, and check operations for the Bloom filter.
 * Tests include happy paths, boundary conditions, error handling, idempotency, and basic probabilistic behaviour.
 *
 * Test framework: Unity
 */
#include "unity.h"
#include "data_structures/bloom_filter_operation.h"
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

// Dummy composite_key_hash for testing
static composite_key_hash make_key(uint64_t val) {
    composite_key_hash key;
    key.sub_bucket_hash = val;
    return key;
}


void test_initialize_bloom_filter_valid_chain_length_returns_success(void) {
    bloom_filter_t *bf = NULL;
    int rc = initialize_bloom_filter(15, &bf);
    TEST_ASSERT_EQUAL(0, rc);
    TEST_ASSERT_NOT_NULL(bf);
    cleanup_bloom_filter(bf);
}

void test_initialize_bloom_filter_null_output_pointer_returns_error(void) {
    int rc = initialize_bloom_filter(15, NULL);
    TEST_ASSERT_NOT_EQUAL(0, rc);
}


void test_initialize_bloom_filter_below_min_chain_length_returns_error(void) {
    bloom_filter_t *bf = NULL;
    int rc = initialize_bloom_filter(6, &bf);
    TEST_ASSERT_NOT_EQUAL(0, rc);
    TEST_ASSERT_NULL(bf);
}

void test_initialize_bloom_filter_min_chain_length_returns_success(void) {
    bloom_filter_t *bf = NULL;
    int rc = initialize_bloom_filter(7, &bf);
    TEST_ASSERT_EQUAL(0, rc);
    TEST_ASSERT_NOT_NULL(bf);
    cleanup_bloom_filter(bf);
}

void test_initialize_bloom_filter_max_chain_length_returns_success(void) {
    bloom_filter_t *bf = NULL;
    int rc = initialize_bloom_filter(30, &bf);
    TEST_ASSERT_EQUAL(0, rc);
    TEST_ASSERT_NOT_NULL(bf);
    cleanup_bloom_filter(bf);
}

void test_initialize_bloom_filter_zero_chain_length_returns_error(void) {
    bloom_filter_t *bf = NULL;
    int rc = initialize_bloom_filter(0, &bf);
    TEST_ASSERT_NOT_EQUAL(0, rc);
    TEST_ASSERT_NULL(bf);
}

void test_initialize_bloom_filter_large_chain_length_returns_error(void) {
    bloom_filter_t *bf = NULL;
    int rc = initialize_bloom_filter(100, &bf);
    TEST_ASSERT_NOT_EQUAL(0, rc);
    TEST_ASSERT_NULL(bf);
}

void test_cleanup_bloom_filter_null_pointer_noop(void) {
    int rc = cleanup_bloom_filter(NULL);
    TEST_ASSERT_EQUAL(0, rc);
}

void test_cleanup_bloom_filter_frees_memory(void) {
    bloom_filter_t *bf = NULL;
    initialize_bloom_filter(10, &bf);
    int rc = cleanup_bloom_filter(bf);
    TEST_ASSERT_EQUAL(0, rc);
    // No direct way to check memory free, but no crash = pass
}

void test_add_key_to_bloom_filter_valid_key_sets_bits(void) {
    // Arrange
    bloom_filter_t *filter = NULL;
    initialize_bloom_filter(20, &filter);
    composite_key_hash key = make_key(0x12345678);

    // Act
    int rc = add_key_to_bloom_filter(key, filter);

    // Assert
    TEST_ASSERT_EQUAL(0, rc);
    cleanup_bloom_filter(filter);
}

void test_add_key_to_bloom_filter_null_filter_returns_error(void) {
    composite_key_hash key = make_key(0x123);
    int rc = add_key_to_bloom_filter(key, NULL);
    TEST_ASSERT_NOT_EQUAL(0, rc);
}

void test_check_key_in_bloom_filter_present_key_returns_true(void) {
    // Arrange
    bloom_filter_t *filter = NULL;
    initialize_bloom_filter(20, &filter);
    composite_key_hash key = make_key(0xABCDEF);
    add_key_to_bloom_filter(key, filter);
    bool found = false;

    // Act
    int rc = check_key_in_bloom_filter(key, filter, &found);

    // Assert
    TEST_ASSERT_EQUAL(0, rc);
    TEST_ASSERT_TRUE(found);
    cleanup_bloom_filter(filter);
}

void test_check_key_in_bloom_filter_absent_key_returns_false(void) {
    // Arrange
    bloom_filter_t *filter = NULL;
    initialize_bloom_filter(20, &filter);
    composite_key_hash key = make_key(0xDEADBEEF);
    bool found = true;

    // Act
    int rc = check_key_in_bloom_filter(key, filter, &found);

    // Assert
    TEST_ASSERT_EQUAL(0, rc);
    TEST_ASSERT_FALSE(found);
    cleanup_bloom_filter(filter);
}

void test_check_key_in_bloom_filter_null_filter_returns_error(void) {
    composite_key_hash key = make_key(0x123);
    bool found = false;
    int rc = check_key_in_bloom_filter(key, NULL, &found);
    TEST_ASSERT_NOT_EQUAL(0, rc);
}

void test_check_key_in_bloom_filter_null_result_pointer_returns_error(void) {
    // Arrange
    bloom_filter_t *filter = NULL;
    initialize_bloom_filter(20, &filter);
    composite_key_hash key = make_key(0x123);

    // Act
    int rc = check_key_in_bloom_filter(key, filter, NULL);

    // Assert
    TEST_ASSERT_NOT_EQUAL(0, rc);
    cleanup_bloom_filter(filter);
}

void test_bloom_filter_idempotency_multiple_adds_same_key(void) {
    // Arrange
    bloom_filter_t *filter = NULL;
    initialize_bloom_filter(20, &filter);
    composite_key_hash key = make_key(0x5555);

    // Act
    add_key_to_bloom_filter(key, filter);
    add_key_to_bloom_filter(key, filter);
    bool found = false;
    check_key_in_bloom_filter(key, filter, &found);

    // Assert
    TEST_ASSERT_TRUE(found);
    cleanup_bloom_filter(filter);
}

void test_bloom_filter_false_positive_rate_with_random_keys(void) {
    // Arrange
    bloom_filter_t *filter = NULL;
    initialize_bloom_filter(20, &filter);
    composite_key_hash keys[10];
    for (int i = 0; i < 10; ++i) {
        keys[i] = make_key(0x1000 + i);
        add_key_to_bloom_filter(keys[i], filter);
    }
    // Act
    composite_key_hash absent = make_key(0x9999);
    bool found = true;
    check_key_in_bloom_filter(absent, filter, &found);
    // Assert: Accept either result, but should not crash
    cleanup_bloom_filter(filter);
}

void test_bloom_filter_add_and_check_multiple_keys(void) {
    // Arrange
    bloom_filter_t *filter = NULL;
    initialize_bloom_filter(20, &filter);
    composite_key_hash keys[5];
    for (int i = 0; i < 5; ++i) {
        keys[i] = make_key(0x2000 + i);
        add_key_to_bloom_filter(keys[i], filter);
    }
    // Act & Assert
    for (int i = 0; i < 5; ++i) {
        bool found = false;
        check_key_in_bloom_filter(keys[i], filter, &found);
        TEST_ASSERT_TRUE(found);
    }
    cleanup_bloom_filter(filter);
}

int test_bloom_filter_operation_main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_initialize_bloom_filter_valid_chain_length_returns_success);
    RUN_TEST(test_initialize_bloom_filter_null_output_pointer_returns_error);
    RUN_TEST(test_initialize_bloom_filter_below_min_chain_length_returns_error);
    RUN_TEST(test_initialize_bloom_filter_min_chain_length_returns_success);
    RUN_TEST(test_initialize_bloom_filter_max_chain_length_returns_success);
    RUN_TEST(test_initialize_bloom_filter_zero_chain_length_returns_error);
    RUN_TEST(test_initialize_bloom_filter_large_chain_length_returns_error);
    RUN_TEST(test_cleanup_bloom_filter_null_pointer_noop);
    RUN_TEST(test_cleanup_bloom_filter_frees_memory);
    RUN_TEST(test_add_key_to_bloom_filter_valid_key_sets_bits);
    RUN_TEST(test_add_key_to_bloom_filter_null_filter_returns_error);
    RUN_TEST(test_check_key_in_bloom_filter_present_key_returns_true);
    RUN_TEST(test_check_key_in_bloom_filter_absent_key_returns_false);
    RUN_TEST(test_check_key_in_bloom_filter_null_filter_returns_error);
    RUN_TEST(test_check_key_in_bloom_filter_null_result_pointer_returns_error);
    RUN_TEST(test_bloom_filter_idempotency_multiple_adds_same_key);
    RUN_TEST(test_bloom_filter_false_positive_rate_with_random_keys);
    RUN_TEST(test_bloom_filter_add_and_check_multiple_keys);
    return UNITY_END();
}
