#include "unity.h"
#include "core/key_store.h"
#include <string.h>
#include <stdlib.h>

hash_table_configuration get_valid_config() {
    hash_table_configuration config = {
        .bucket_size = 8,
        .is_concurrency_enabled = false,
        .sub_hash_table_bucket_size = 4,
        .max_linked_list_chain_length = 4
    };
    return config;
}

void test_initialise_key_store_success(void) {
    hash_table_configuration config = get_valid_config();
    int res = initialise_key_store(config, 1.0);
    TEST_ASSERT_EQUAL(0, res);
    cleanup_key_store();
}

void test_initialise_key_store_invalid_config(void) {
    hash_table_configuration config = get_valid_config();
    config.bucket_size = 0;
    TEST_ASSERT_LESS_THAN(0, initialise_key_store(config, 1.0));
    config = get_valid_config();
    config.sub_hash_table_bucket_size = 0;
    TEST_ASSERT_LESS_THAN(0, initialise_key_store(config, 1.0));
    config = get_valid_config();
    config.max_linked_list_chain_length = 0;
    TEST_ASSERT_LESS_THAN(0, initialise_key_store(config, 1.0));
    config = get_valid_config();
    TEST_ASSERT_LESS_THAN(0, initialise_key_store(config, -0.1));
    TEST_ASSERT_LESS_THAN(0, initialise_key_store(config, 1.1));
    config.bucket_size = 7; // not power of two
    TEST_ASSERT_LESS_THAN(0, initialise_key_store(config, 1.0));
    config = get_valid_config();
    config.sub_hash_table_bucket_size = 3; // not power of two
    TEST_ASSERT_LESS_THAN(0, initialise_key_store(config, 1.0));
}

void test_cleanup_key_store_success(void) {
    hash_table_configuration config = get_valid_config();
    TEST_ASSERT_EQUAL(0, initialise_key_store(config, 1.0));
    int res = cleanup_key_store();
    TEST_ASSERT_EQUAL(0, res);
}

void test_set_key_and_get_key_success(void) {
    hash_table_configuration config = get_valid_config();
    TEST_ASSERT_EQUAL(0, initialise_key_store(config, 1.0));

    // String value
    unsigned char str_value[] = "val";
    key_value_pair kv_str = {"mykey", str_value, strlen((char*)str_value) + 1};
    TEST_ASSERT_EQUAL(0, set_key(&kv_str));
    key_value_pair out_str = {0};
    TEST_ASSERT_EQUAL(0, get_key("mykey", &out_str));
    TEST_ASSERT_EQUAL_STRING("mykey", out_str.key);
    TEST_ASSERT_EQUAL_STRING("val", out_str.value);

    // Integer value
    int int_val = 123456;
    key_value_pair kv_int = {"intkey", (unsigned char*)&int_val, sizeof(int)};
    TEST_ASSERT_EQUAL(0, set_key(&kv_int));
    key_value_pair out_int = {0};
    TEST_ASSERT_EQUAL(0, get_key("intkey", &out_int));
    TEST_ASSERT_EQUAL_STRING("intkey", out_int.key);
    TEST_ASSERT_EQUAL(sizeof(int), out_int.value_size);
    TEST_ASSERT_EQUAL(int_val, *(int*)out_int.value);

    // Float value
    float float_val = 3.14159f;
    key_value_pair kv_float = {"floatkey", (unsigned char*)&float_val, sizeof(float)};
    TEST_ASSERT_EQUAL(0, set_key(&kv_float));
    key_value_pair out_float = {0};
    TEST_ASSERT_EQUAL(0, get_key("floatkey", &out_float));
    TEST_ASSERT_EQUAL_STRING("floatkey", out_float.key);
    TEST_ASSERT_EQUAL(sizeof(float), out_float.value_size);
    TEST_ASSERT_TRUE(memcmp(&float_val, out_float.value, sizeof(float)) == 0);

    // Bytes value (arbitrary binary data)
    unsigned char bytes_val[] = {0xDE, 0xAD, 0xBE, 0xEF};
    key_value_pair kv_bytes = {"byteskey", bytes_val, sizeof(bytes_val)};
    TEST_ASSERT_EQUAL(0, set_key(&kv_bytes));
    key_value_pair out_bytes = {0};
    TEST_ASSERT_EQUAL(0, get_key("byteskey", &out_bytes));
    TEST_ASSERT_EQUAL_STRING("byteskey", out_bytes.key);
    TEST_ASSERT_EQUAL(sizeof(bytes_val), out_bytes.value_size);
    TEST_ASSERT_TRUE(memcmp(bytes_val, out_bytes.value, sizeof(bytes_val)) == 0);

    // Double value
    double double_val = 2.718281828459;
    key_value_pair kv_double = {"doublekey", (unsigned char*)&double_val, sizeof(double)};
    TEST_ASSERT_EQUAL(0, set_key(&kv_double));
    key_value_pair out_double = {0};
    TEST_ASSERT_EQUAL(0, get_key("doublekey", &out_double));
    TEST_ASSERT_EQUAL_STRING("doublekey", out_double.key);
    TEST_ASSERT_EQUAL(sizeof(double), out_double.value_size);
    TEST_ASSERT_TRUE(memcmp(&double_val, out_double.value, sizeof(double)) == 0);

    // Long long value
    long long ll_val = 0x123456789ABCDEF0LL;
    key_value_pair kv_ll = {"llkey", (unsigned char*)&ll_val, sizeof(long long)};
    TEST_ASSERT_EQUAL(0, set_key(&kv_ll));
    key_value_pair out_ll = {0};
    TEST_ASSERT_EQUAL(0, get_key("llkey", &out_ll));
    TEST_ASSERT_EQUAL_STRING("llkey", out_ll.key);
    TEST_ASSERT_EQUAL(sizeof(long long), out_ll.value_size);
    TEST_ASSERT_TRUE(memcmp(&ll_val, out_ll.value, sizeof(long long)) == 0);

    // Bool value
    _Bool bool_val = 1;
    key_value_pair kv_bool = {"boolkey", (unsigned char*)&bool_val, sizeof(_Bool)};
    TEST_ASSERT_EQUAL(0, set_key(&kv_bool));
    key_value_pair out_bool = {0};
    TEST_ASSERT_EQUAL(0, get_key("boolkey", &out_bool));
    TEST_ASSERT_EQUAL_STRING("boolkey", out_bool.key);
    TEST_ASSERT_EQUAL(sizeof(_Bool), out_bool.value_size);
    TEST_ASSERT_TRUE(memcmp(&bool_val, out_bool.value, sizeof(_Bool)) == 0);

    // Enum value
    enum Color { RED = 1, GREEN = 2, BLUE = 3 };
    enum Color color_val = GREEN;
    key_value_pair kv_enum = {"colorkey", (unsigned char*)&color_val, sizeof(enum Color)};
    TEST_ASSERT_EQUAL(0, set_key(&kv_enum));
    key_value_pair out_enum = {0};
    TEST_ASSERT_EQUAL(0, get_key("colorkey", &out_enum));
    TEST_ASSERT_EQUAL_STRING("colorkey", out_enum.key);
    TEST_ASSERT_EQUAL(sizeof(enum Color), out_enum.value_size);
    TEST_ASSERT_TRUE(memcmp(&color_val, out_enum.value, sizeof(enum Color)) == 0);

    // Struct value
    struct Point { int x; float y; };
    struct Point pt = {42, 1.5f};
    key_value_pair kv_struct = {"structkey", (unsigned char*)&pt, sizeof(struct Point)};
    TEST_ASSERT_EQUAL(0, set_key(&kv_struct));
    key_value_pair out_struct = {0};
    TEST_ASSERT_EQUAL(0, get_key("structkey", &out_struct));
    TEST_ASSERT_EQUAL_STRING("structkey", out_struct.key);
    TEST_ASSERT_EQUAL(sizeof(struct Point), out_struct.value_size);
    TEST_ASSERT_TRUE(memcmp(&pt, out_struct.value, sizeof(struct Point)) == 0);

    cleanup_key_store();
}

void test_set_key_update_existing(void) {
    hash_table_configuration config = get_valid_config();
    initialise_key_store(config, 1.0);

    unsigned char value1[] = "v1";
    key_value_pair kv = {"dupkey", value1, strlen((char*)value1) + 1};
    set_key(&kv);

    unsigned char value2[] = "v2";
    kv.value = value2;
    kv.value_size = strlen((char*)value2) + 1;
    TEST_ASSERT_EQUAL(0, set_key(&kv));

    key_value_pair out = {0};
    TEST_ASSERT_EQUAL(0, get_key("dupkey", &out));
    TEST_ASSERT_EQUAL_STRING("v2", out.value);

    cleanup_key_store();
}

void test_set_key_invalid_args(void) {
    hash_table_configuration config = get_valid_config();
    initialise_key_store(config, 1.0);

    unsigned char value[] = "val";
    key_value_pair kv = {NULL, value, strlen((char*)value) + 1};
    TEST_ASSERT_LESS_THAN(0, set_key(&kv));
    kv.key = "";
    TEST_ASSERT_LESS_THAN(0, set_key(&kv));
    kv.key = "k";
    kv.value = NULL;
    TEST_ASSERT_LESS_THAN(0, set_key(&kv));
    kv.value = value;
    kv.value_size = 0;
    TEST_ASSERT_LESS_THAN(0, set_key(&kv));
    TEST_ASSERT_LESS_THAN(0, set_key(NULL));

    cleanup_key_store();
}

void test_get_key_invalid_args(void) {
    hash_table_configuration config = get_valid_config();
    initialise_key_store(config, 1.0);

    key_value_pair out = {0};
    TEST_ASSERT_LESS_THAN(0, get_key(NULL, &out));
    TEST_ASSERT_LESS_THAN(0, get_key("", &out));
    TEST_ASSERT_LESS_THAN(0, get_key("k", NULL));

    cleanup_key_store();
}

void test_get_key_not_found(void) {
    hash_table_configuration config = get_valid_config();
    initialise_key_store(config, 1.0);

    key_value_pair out = {0};
    TEST_ASSERT_LESS_THAN(0, get_key("notfound", &out));

    cleanup_key_store();
}

void test_delete_key_success_and_not_found(void) {
    hash_table_configuration config = get_valid_config();
    initialise_key_store(config, 1.0);

    unsigned char value[] = "val";
    key_value_pair kv = {"delkey", value, strlen((char*)value) + 1};
    set_key(&kv);

    TEST_ASSERT_EQUAL(0, delete_key("delkey"));
    TEST_ASSERT_LESS_THAN(0, delete_key("delkey")); // already deleted

    cleanup_key_store();
}

void test_delete_key_invalid_args(void) {
    hash_table_configuration config = get_valid_config();
    initialise_key_store(config, 1.0);

    TEST_ASSERT_LESS_THAN(0, delete_key(NULL));
    TEST_ASSERT_LESS_THAN(0, delete_key(""));

    cleanup_key_store();
}

int test_key_store_main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_initialise_key_store_success);
    RUN_TEST(test_initialise_key_store_invalid_config);
    RUN_TEST(test_cleanup_key_store_success);
    RUN_TEST(test_set_key_and_get_key_success);
    RUN_TEST(test_set_key_update_existing);
    RUN_TEST(test_set_key_invalid_args);
    RUN_TEST(test_get_key_invalid_args);
    RUN_TEST(test_get_key_not_found);
    RUN_TEST(test_delete_key_success_and_not_found);
    RUN_TEST(test_delete_key_invalid_args);
    return UNITY_END();
}