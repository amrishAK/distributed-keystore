/**
 * @file test_hash_functions.c
 * @brief Unit tests for hash functions using the Unity framework.
 *
 * This file contains unit tests focused on improving code coverage for the
 * hash_function_murmur_32 function. The scenarios tested include:
 * - Basic hashing of a typical string.
 * - Hashing of an empty string.
 * - Hashing of different keys to ensure different outputs.
 * - Hashing with different seed values.
 * - Hashing when the key is NULL.
 * - Hashing of a long string (large buffer).
 * - Hashing with seed value zero.
 * - Hashing of a single character string.
 * - Hashing with maximum seed value (UINT32_MAX).
 * - Hashing with minimum seed value (0).
 * - Hashing of a string containing special characters.
 * - Hashing of a UTF-8 encoded (unicode) string.
 * - Hashing of a string with repeated characters.
 * - Hashing of binary data (non-printable characters).
 */

#include "unity.h"
#include "hash/hash_functions.h"
#include <string.h>
#include <limits.h>

void setUp(void) {}
void tearDown(void) {}

void test_murmurhash_basic(void) {
    const char *key = "testkey";
    uint32_t seed = 42;
    uint32_t hash = hash_function_murmur_32(key, seed);
    TEST_ASSERT_NOT_EQUAL(0, hash);
    TEST_ASSERT_EQUAL(hash, hash_function_murmur_32(key, seed));
}

void test_murmurhash_empty_string(void) {
    const char *key = "";
    uint32_t seed = 42;
    uint32_t hash = hash_function_murmur_32(key, seed);
    TEST_ASSERT_EQUAL(hash, hash_function_murmur_32(key, seed));
}

void test_murmurhash_different_keys(void) {
    const char *key1 = "key1";
    const char *key2 = "key2";
    uint32_t seed = 42;
    uint32_t hash1 = hash_function_murmur_32(key1, seed);
    uint32_t hash2 = hash_function_murmur_32(key2, seed);
    TEST_ASSERT_NOT_EQUAL(hash1, hash2);
}

void test_murmurhash_different_seeds(void) {
    const char *key = "testkey";
    uint32_t hash1 = hash_function_murmur_32(key, 1);
    uint32_t hash2 = hash_function_murmur_32(key, 2);
    TEST_ASSERT_NOT_EQUAL(hash1, hash2);
}

void test_murmurhash_null_key(void) {
    uint32_t seed = 42;
    uint32_t hash = hash_function_murmur_32(NULL, seed);
    TEST_ASSERT_EQUAL(0, hash);
}

void test_murmurhash_long_string(void) {
    char key[1024];
    memset(key, 'A', sizeof(key) - 1);
    key[sizeof(key) - 1] = '\0';
    uint32_t seed = 123;
    uint32_t hash = hash_function_murmur_32(key, seed);
    TEST_ASSERT_NOT_EQUAL(0, hash);
    TEST_ASSERT_EQUAL(hash, hash_function_murmur_32(key, seed));
}

void test_murmurhash_seed_zero(void) {
    const char *key = "seedzero";
    uint32_t hash1 = hash_function_murmur_32(key, 0);
    uint32_t hash2 = hash_function_murmur_32(key, 0);
    TEST_ASSERT_EQUAL(hash1, hash2);
    TEST_ASSERT_NOT_EQUAL(0, hash1);
}

void test_murmurhash_single_char(void) {
    const char *key = "A";
    uint32_t seed = 99;
    uint32_t hash = hash_function_murmur_32(key, seed);
    TEST_ASSERT_NOT_EQUAL(0, hash);
    TEST_ASSERT_EQUAL(hash, hash_function_murmur_32(key, seed));
}

// Additional tests for improved coverage

void test_murmurhash_max_seed(void) {
    const char *key = "maxseed";
    uint32_t hash1 = hash_function_murmur_32(key, UINT32_MAX);
    uint32_t hash2 = hash_function_murmur_32(key, UINT32_MAX);
    TEST_ASSERT_EQUAL(hash1, hash2);
    TEST_ASSERT_NOT_EQUAL(0, hash1);
}

void test_murmurhash_min_seed(void) {
    const char *key = "minseed";
    uint32_t hash1 = hash_function_murmur_32(key, 0);
    uint32_t hash2 = hash_function_murmur_32(key, 0);
    TEST_ASSERT_EQUAL(hash1, hash2);
    TEST_ASSERT_NOT_EQUAL(0, hash1);
}

void test_murmurhash_special_chars(void) {
    const char *key = "!@#$%^&*()_+-=[]{}|;':,.<>/?";
    uint32_t seed = 12345;
    uint32_t hash = hash_function_murmur_32(key, seed);
    TEST_ASSERT_NOT_EQUAL(0, hash);
    TEST_ASSERT_EQUAL(hash, hash_function_murmur_32(key, seed));
}

void test_murmurhash_unicode(void) {
    // UTF-8 encoded string
    const char *key = "测试🌟";
    uint32_t seed = 9876;
    uint32_t hash = hash_function_murmur_32(key, seed);
    TEST_ASSERT_NOT_EQUAL(0, hash);
    TEST_ASSERT_EQUAL(hash, hash_function_murmur_32(key, seed));
}

void test_murmurhash_repeated_chars(void) {
    char key[256];
    memset(key, 'B', sizeof(key) - 1);
    key[sizeof(key) - 1] = '\0';
    uint32_t seed = 555;
    uint32_t hash = hash_function_murmur_32(key, seed);
    TEST_ASSERT_NOT_EQUAL(0, hash);
    TEST_ASSERT_EQUAL(hash, hash_function_murmur_32(key, seed));
}

void test_murmurhash_binary_data(void) {
    unsigned char key[] = {0x00, 0xFF, 0xAA, 0x55, 0x10, 0x20, 0x30, 0x40};
    // Treat as string with length 8
    uint32_t seed = 321;
    // Cast to char* and ensure null-termination for string-based hash
    char key_str[9];
    memcpy(key_str, key, 8);
    key_str[8] = '\0';
    uint32_t hash = hash_function_murmur_32(key_str, seed);
    TEST_ASSERT_NOT_EQUAL(0, hash);
    TEST_ASSERT_EQUAL(hash, hash_function_murmur_32(key_str, seed));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_murmurhash_basic);
    RUN_TEST(test_murmurhash_empty_string);
    RUN_TEST(test_murmurhash_different_keys);
    RUN_TEST(test_murmurhash_different_seeds);
    RUN_TEST(test_murmurhash_null_key);
    RUN_TEST(test_murmurhash_long_string);
    RUN_TEST(test_murmurhash_seed_zero);
    RUN_TEST(test_murmurhash_single_char);
    RUN_TEST(test_murmurhash_max_seed);
    RUN_TEST(test_murmurhash_min_seed);
    RUN_TEST(test_murmurhash_special_chars);
    RUN_TEST(test_murmurhash_unicode);
    RUN_TEST(test_murmurhash_repeated_chars);
    RUN_TEST(test_murmurhash_binary_data);
    return UNITY_END();
}
