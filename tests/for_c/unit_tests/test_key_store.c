
#include "unity.h"
#include "core/key_store.h"
#include <string.h>
#include <limits.h>

// Helper for freeing key_store_value
static void free_key_store_value(key_store_value *value) {
    if (value && value->data) {
        free((void *)value->data);
        value->data = NULL;
        value->data_size = 0;
    }
}

void test_set_binary_data(void) {
    initialise_key_store(8, 0.5);
    unsigned char bin[] = {0x01, 0x02, 0x03, 0x04};
    key_store_value value = {bin, sizeof(bin)};
    TEST_ASSERT_EQUAL(0, set_key("bin", value));
    cleanup_key_store();
}

void test_update_binary_data(void) {
    initialise_key_store(8, 0.5);
    unsigned char bin[] = {0x01, 0x02};
    key_store_value value = {bin, sizeof(bin)};
    set_key("bin", value);
    unsigned char bin2[] = {0xFF, 0xEE};
    key_store_value value2 = {bin2, sizeof(bin2)};
    TEST_ASSERT_EQUAL(0, set_key("bin", value2));
    key_store_value out = {0};
    TEST_ASSERT_EQUAL(0, get_key("bin", &out));
    TEST_ASSERT_EQUAL(out.data_size, sizeof(bin2));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(bin2, out.data, out.data_size);
    free_key_store_value(&out);
    cleanup_key_store();
}

void test_delete_binary_data(void) {
    initialise_key_store(8, 0.5);
    unsigned char bin[] = {0xAA, 0xBB};
    key_store_value value = {bin, sizeof(bin)};
    set_key("bin", value);
    TEST_ASSERT_EQUAL(0, delete_key("bin"));
    key_store_value out = {0};
    TEST_ASSERT_EQUAL(-1, get_key("bin", &out));
    cleanup_key_store();
}

void test_get_binary_data(void) {
    initialise_key_store(8, 0.5);
    unsigned char bin[] = {0x10, 0x20, 0x30};
    key_store_value value = {bin, sizeof(bin)};
    set_key("bin", value);
    key_store_value out = {0};
    TEST_ASSERT_EQUAL(0, get_key("bin", &out));
    TEST_ASSERT_EQUAL(out.data_size, sizeof(bin));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(bin, out.data, out.data_size);
    free_key_store_value(&out);
    cleanup_key_store();
}

void test_initialise_key_store(void) {
    TEST_ASSERT_EQUAL(0, initialise_key_store(8, 0.5));
    cleanup_key_store();
}

void test_update_key(void) {
    initialise_key_store(8, 0.5);
    key_store_value value = {(const unsigned char *)"abc", 3};
    set_key("key", value);
    key_store_value value2 = {(const unsigned char *)"def", 3};
    TEST_ASSERT_EQUAL(0, set_key("key", value2));
    key_store_value out = {0};
    TEST_ASSERT_EQUAL(0, get_key("key", &out));
    TEST_ASSERT_EQUAL_STRING_LEN("def", out.data, 3);
    free_key_store_value(&out);
    cleanup_key_store();
}

void test_delete_key(void) {
    initialise_key_store(8, 0.5);
    key_store_value value = {(const unsigned char *)"abc", 3};
    set_key("key", value);
    TEST_ASSERT_EQUAL(0, delete_key("key"));
    key_store_value out = {0};
    TEST_ASSERT_EQUAL(-1, get_key("key", &out));
    cleanup_key_store();
}

void test_invalid_key(void) {
    initialise_key_store(8, 0.5);
    key_store_value value = {(const unsigned char *)"abc", 3};
    TEST_ASSERT_EQUAL(-1, set_key(NULL, value));
    TEST_ASSERT_EQUAL(-1, set_key("", value));
    key_store_value out = {0};
    TEST_ASSERT_EQUAL(-1, get_key(NULL, &out));
    TEST_ASSERT_EQUAL(-1, get_key("", &out));
    TEST_ASSERT_EQUAL(-1, delete_key(NULL));
    TEST_ASSERT_EQUAL(-1, delete_key("") );
    cleanup_key_store();
}

void test_collision_handling(void) {
    initialise_key_store(8, 0.5);
    key_store_value v1 = {(const unsigned char *)"data1", 6};
    key_store_value v2 = {(const unsigned char *)"data2", 6};
    set_key("keyA", v1);
    set_key("keyB", v2);
    key_store_value out1 = {0}, out2 = {0};
    TEST_ASSERT_EQUAL(0, get_key("keyA", &out1));
    TEST_ASSERT_EQUAL(0, get_key("keyB", &out2));
    free_key_store_value(&out1);
    free_key_store_value(&out2);
    cleanup_key_store();
}

void test_repeated_set_delete(void) {
    initialise_key_store(8, 0.5);
    key_store_value value = {(const unsigned char *)"abc", 3};
    for(int i=0; i<10; ++i) {
        TEST_ASSERT_EQUAL(0, set_key("key", value));
        TEST_ASSERT_EQUAL(0, delete_key("key"));
    }
    cleanup_key_store();
}

void test_delete_nonexistent_key(void) {
    initialise_key_store(8, 0.5);
    TEST_ASSERT_EQUAL(-1, delete_key("nope"));
    cleanup_key_store();
}

void test_set_key_after_delete(void) {
    initialise_key_store(8, 0.5);
    key_store_value value = {(const unsigned char *)"abc", 3};
    set_key("key", value);
    delete_key("key");
    TEST_ASSERT_EQUAL(0, set_key("key", value));
    key_store_value out = {0};
    TEST_ASSERT_EQUAL(0, get_key("key", &out));
    TEST_ASSERT_EQUAL_STRING_LEN("abc", out.data, 3);
    free_key_store_value(&out);
    cleanup_key_store();
}

void test_long_key_and_data_to_store(void) {
    initialise_key_store(8, 0.5);
    char long_key[256];
    memset(long_key, 'A', sizeof(long_key)-1);
    long_key[255] = '\0';
    unsigned char long_data[512];
    for(size_t i=0; i<sizeof(long_data); ++i) long_data[i] = (unsigned char)(i%256);
    key_store_value value = {long_data, sizeof(long_data)};
    TEST_ASSERT_EQUAL(0, set_key(long_key, value));
    key_store_value out = {0};
    TEST_ASSERT_EQUAL(0, get_key(long_key, &out));
    TEST_ASSERT_EQUAL(out.data_size, sizeof(long_data));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(long_data, out.data, out.data_size);
    free_key_store_value(&out);
    cleanup_key_store();
}

void test_min_max_key(void) {
    initialise_key_store(8, 0.5);
    char min_key[2] = "a";
    char max_key[256];
    memset(max_key, 'Z', sizeof(max_key) - 1);
    max_key[255] = '\0';
    key_store_value minv = {(const unsigned char *)"min", 3};
    key_store_value maxv = {(const unsigned char *)"max", 3};
    TEST_ASSERT_EQUAL(0, set_key(min_key, minv));
    TEST_ASSERT_EQUAL(0, set_key(max_key, maxv));
    key_store_value out1 = {0}, out2 = {0};
    TEST_ASSERT_EQUAL(0, get_key(min_key, &out1));
    TEST_ASSERT_EQUAL(0, get_key(max_key, &out2));
    free_key_store_value(&out1);
    free_key_store_value(&out2);
    cleanup_key_store();
}

void test_overwrite_with_different_size(void) {
    initialise_key_store(8, 0.5);
    key_store_value v1 = {(const unsigned char *)"abc", 3};
    key_store_value v2 = {(const unsigned char *)"abcdef", 6};
    set_key("key", v1);
    set_key("key", v2);
    key_store_value out = {0};
    TEST_ASSERT_EQUAL(0, get_key("key", &out));
    TEST_ASSERT_EQUAL_STRING_LEN("abcdef", out.data, 6);
    free_key_store_value(&out);
    cleanup_key_store();
}

void test_many_keys(void) {
    initialise_key_store(32, 0.5);
    char key[16];
    unsigned char val[8];
    for(int i=0; i<100; ++i) {
        snprintf(key, sizeof(key), "k%d", i);
        for(int j=0; j<8; ++j) val[j] = (unsigned char)(i+j);
        key_store_value v = {val, sizeof(val)};
        TEST_ASSERT_EQUAL(0, set_key(key, v));
    }
    for(int i=0; i<100; ++i) {
        snprintf(key, sizeof(key), "k%d", i);
        key_store_value out = {0};
        TEST_ASSERT_EQUAL(0, get_key(key, &out));
        free_key_store_value(&out);
    }
    cleanup_key_store();
}

void test_null_and_zero_data(void) {
    initialise_key_store(8, 0.5);
    key_store_value vnull = {NULL, 0};
    TEST_ASSERT_EQUAL(-1, set_key("key", vnull));
    key_store_value vzero = {(const unsigned char *)"", 0};
    TEST_ASSERT_EQUAL(-1, set_key("key", vzero));
    cleanup_key_store();
}

void test_set_get_multiple_keys(void) {
    initialise_key_store(8, 0.5);
    key_store_value v1 = {(const unsigned char *)"one", 3};
    key_store_value v2 = {(const unsigned char *)"two", 3};
    key_store_value v3 = {(const unsigned char *)"three", 5};
    set_key("k1", v1);
    set_key("k2", v2);
    set_key("k3", v3);
    key_store_value out1 = {0}, out2 = {0}, out3 = {0};
    TEST_ASSERT_EQUAL(0, get_key("k1", &out1));
    TEST_ASSERT_EQUAL(0, get_key("k2", &out2));
    TEST_ASSERT_EQUAL(0, get_key("k3", &out3));
    TEST_ASSERT_EQUAL_STRING_LEN("one", out1.data, 3);
    TEST_ASSERT_EQUAL_STRING_LEN("two", out2.data, 3);
    TEST_ASSERT_EQUAL_STRING_LEN("three", out3.data, 5);
    free_key_store_value(&out1);
    free_key_store_value(&out2);
    free_key_store_value(&out3);
    cleanup_key_store();
}



int test_key_store_suite(void) {
    printf("Running key_store tests...\n");
    RUN_TEST(test_set_binary_data);
    RUN_TEST(test_update_binary_data);
    RUN_TEST(test_delete_binary_data);
    RUN_TEST(test_get_binary_data);
    RUN_TEST(test_initialise_key_store);
    RUN_TEST(test_update_key);
    RUN_TEST(test_delete_key);
    RUN_TEST(test_invalid_key);
    RUN_TEST(test_collision_handling);
    RUN_TEST(test_repeated_set_delete);
    RUN_TEST(test_delete_nonexistent_key);
    RUN_TEST(test_set_key_after_delete);
    RUN_TEST(test_long_key_and_data_to_store);
    RUN_TEST(test_min_max_key);
    RUN_TEST(test_overwrite_with_different_size);
    RUN_TEST(test_many_keys);
    RUN_TEST(test_null_and_zero_data);
    RUN_TEST(test_set_get_multiple_keys);
    printf("Completed key_store tests.\n");
    return 0;
}