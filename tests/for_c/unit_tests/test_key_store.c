
#include "unity.h"
#include "core/key_store.h"
#include <string.h>
#include <limits.h>

void test_initialise_key_store(void) {
    TEST_ASSERT_EQUAL(0, initialise_key_store(8));
    TEST_ASSERT_EQUAL(-1, initialise_key_store(0));
    TEST_ASSERT_EQUAL(-1, initialise_key_store(-8));
    TEST_ASSERT_EQUAL(-1, initialise_key_store(7)); // not power of two
}

void test_set_get_multiple_keys(void) {
    initialise_key_store(32);
    const char *keys[] = {"key1", "key2", "key3", "key4", "key5", "key6"};
    const char *data[] = {"key1", "key2", "key3", "key4", "key5", "key6"};

    for (int i = 0; i < 6; ++i) {
        const unsigned char *data_ptr = (const unsigned char *)data[i];
        int ret = set_key(keys[i], data_ptr, strlen(data[i]) + 1);
        TEST_ASSERT_TRUE_MESSAGE(ret == 0 , "set_key failed unexpectedly");
    }
    for (int i = 0; i < 6; ++i) {
        data_node *node = get_key(keys[i]);
        TEST_ASSERT_NOT_NULL(node);
        TEST_ASSERT_EQUAL_UINT8_ARRAY(data[i], node->data, strlen(data[i]) + 1);
    }
}

void test_update_key(void) {
    initialise_key_store(8);
    const char *key = "mykey";
    unsigned char data1[] = "value1";
    unsigned char data2[] = "value2";

    int result1 = set_key(key, data1, sizeof(data1));
    TEST_ASSERT_EQUAL_MESSAGE(0, result1, "Failed to set key with data1");
    int result2 = set_key(key, data2, sizeof(data2));
    TEST_ASSERT_EQUAL_MESSAGE(0, result2, "Failed to set key with data2");

    data_node *node = get_key(key);
    TEST_ASSERT_NOT_NULL_MESSAGE(node, "Failed to retrieve updated node");
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(data2, node->data, sizeof(data2), "Data mismatch");
}

void test_delete_key(void) {
    initialise_key_store(8);
    const char *key = "mykey";
    unsigned char data[] = "value";
    size_t data_size = sizeof(data);

    set_key(key, data, data_size);
    TEST_ASSERT_EQUAL(0, delete_key(key));
    TEST_ASSERT_NULL(get_key(key));
}

void test_invalid_key(void) {
    initialise_key_store(8);
    unsigned char data[] = "value";
    size_t data_size = sizeof(data);

    TEST_ASSERT_EQUAL(-1, set_key(NULL, data, data_size));
    TEST_ASSERT_EQUAL(-1, set_key("", data, data_size));
    TEST_ASSERT_EQUAL(-1, set_key("valid", NULL, data_size));
    TEST_ASSERT_EQUAL(-1, set_key("valid", data, 0));
    TEST_ASSERT_NULL(get_key(NULL));
    TEST_ASSERT_NULL(get_key(""));
    TEST_ASSERT_EQUAL(-1, delete_key(NULL));
    TEST_ASSERT_EQUAL(-1, delete_key(""));
}

void test_collision_handling(void) {
    initialise_key_store(2); // Small bucket size to force collisions
    const char *key1 = "keyA";
    const char *key2 = "keyB";
    unsigned char data1[] = "dataA";
    unsigned char data2[] = "dataB";
    size_t data_size = sizeof(data1);

    set_key(key1, data1, data_size);
    set_key(key2, data2, data_size);

    data_node *node1 = get_key(key1);
    data_node *node2 = get_key(key2);
    TEST_ASSERT_NOT_NULL(node1);
    TEST_ASSERT_NOT_NULL(node2);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(data1, node1->data, data_size);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(data2, node2->data, data_size);
}

void test_repeated_set_delete(void) {
    initialise_key_store(8);
    const char *key = "repeat";
    unsigned char data[] = "val";
    size_t data_size = sizeof(data);

    for (int i = 0; i < 10; ++i) {
        TEST_ASSERT_EQUAL(0, set_key(key, data, data_size));
        TEST_ASSERT_NOT_NULL(get_key(key));
        TEST_ASSERT_EQUAL(0, delete_key(key));
        TEST_ASSERT_NULL(get_key(key));
    }
}

void test_delete_nonexistent_key(void) {
    initialise_key_store(8);
    TEST_ASSERT_EQUAL(-1, delete_key("notfound"));
}

void test_set_key_after_delete(void) {
    initialise_key_store(8);
    const char *key = "mykey";
    unsigned char data[] = "value";
    size_t data_size = sizeof(data);

    set_key(key, data, data_size);
    delete_key(key);
    TEST_ASSERT_NULL(get_key(key));
    TEST_ASSERT_EQUAL(0, set_key(key, data, data_size));
    TEST_ASSERT_NOT_NULL(get_key(key));
}

void test_long_key_and_data_to_store(void) {
    initialise_key_store(8);
    char key[1024];
    memset(key, 'A', sizeof(key) - 1);
    key[1023] = '\0';
    unsigned char data[2048];
    memset(data, 0xAB, sizeof(data));
    size_t data_size = sizeof(data);

    TEST_ASSERT_EQUAL(0, set_key(key, data, data_size));
    data_node *node = get_key(key);
    TEST_ASSERT_NOT_NULL(node);
    TEST_ASSERT_EQUAL_STRING(key, node->key);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(data, node->data, data_size);
}

void test_min_max_key(void) {
    initialise_key_store(8);
    char min_key[2] = "a";
    char max_key[256];
    memset(max_key, 'Z', sizeof(max_key) - 1);
    max_key[255] = '\0';
    unsigned char data[] = "data";
    size_t data_size = sizeof(data);

    TEST_ASSERT_EQUAL(0, set_key(min_key, data, data_size));
    TEST_ASSERT_EQUAL(0, set_key(max_key, data, data_size));
    TEST_ASSERT_NOT_NULL(get_key(min_key));
    TEST_ASSERT_NOT_NULL(get_key(max_key));
}

void test_overwrite_with_different_size(void) {
    initialise_key_store(8);
    const char *key = "resize";
    unsigned char data1[] = "short";
    unsigned char data2[] = "muchlongerdata";
    set_key(key, data1, sizeof(data1));
    set_key(key, data2, sizeof(data2));
    data_node *node = get_key(key);
    TEST_ASSERT_NOT_NULL(node);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(data2, node->data, sizeof(data2));
}

void test_many_keys(void) {
    initialise_key_store(64);
    char key[16];
    unsigned char data[16];
    for (int i = 0; i < 100; ++i) {
        snprintf(key, sizeof(key), "key%d", i);
        memset(data, i, sizeof(data));
        TEST_ASSERT_EQUAL(0, set_key(key, data, sizeof(data)));
    }
    for (int i = 0; i < 100; ++i) {
        snprintf(key, sizeof(key), "key%d", i);
        data_node *node = get_key(key);
        TEST_ASSERT_NOT_NULL(node);
        TEST_ASSERT_EQUAL_STRING(key, node->key);
    }
}

void test_null_and_zero_data(void) {
    initialise_key_store(8);
    const char *key = "null";
    TEST_ASSERT_EQUAL(-1, set_key(key, NULL, 10));
    unsigned char data[] = "data";
    TEST_ASSERT_EQUAL(-1, set_key(key, data, 0));
}

void test_set_binary_data(void) {
    initialise_key_store(8);
    const char *key = "bin1";
    unsigned char data[] = {0x00, 0xFF, 0x7E, 0x42, 0x00, 0x10};
    size_t data_size = sizeof(data);
    TEST_ASSERT_EQUAL(0, set_key(key, data, data_size));
    data_node *node = get_key(key);
    TEST_ASSERT_NOT_NULL(node);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(data, node->data, data_size);
}

void test_update_binary_data(void) {
    initialise_key_store(8);
    const char *key = "bin2";
    unsigned char data1[] = {0x01, 0x02, 0x03, 0x04};
    unsigned char data2[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
    TEST_ASSERT_EQUAL(0, set_key(key, data1, sizeof(data1)));
    TEST_ASSERT_EQUAL(0, set_key(key, data2, sizeof(data2)));
    data_node *node = get_key(key);
    TEST_ASSERT_NOT_NULL(node);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(data2, node->data, sizeof(data2));
}

void test_delete_binary_data(void) {
    initialise_key_store(8);
    const char *key = "bin3";
    unsigned char data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    size_t data_size = sizeof(data);
    TEST_ASSERT_EQUAL(0, set_key(key, data, data_size));
    TEST_ASSERT_EQUAL(0, delete_key(key));
    TEST_ASSERT_NULL(get_key(key));
}

void test_get_binary_data(void) {
    initialise_key_store(8);
    const char *key = "bin4";
    unsigned char data[] = {0x11, 0x22, 0x33, 0x44, 0x55};
    size_t data_size = sizeof(data);
    TEST_ASSERT_EQUAL(0, set_key(key, data, data_size));
    data_node *node = get_key(key);
    TEST_ASSERT_NOT_NULL(node);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(data, node->data, data_size);
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