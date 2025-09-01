#include "unity.h"
#include "test_hash_functions.c"
#include "test_data_node.c"
#include "test_hash_bucket_list.c"

void setUp(void) {}
void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    test_hash_functions_suite();
    test_data_node_suite();
    test_hash_bucket_list_suite();
    return UNITY_END();
}
