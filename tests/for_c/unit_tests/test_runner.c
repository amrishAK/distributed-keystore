#include "unity.h"
#include "test_hash_functions.c"
#include "test_data_node.c"

void setUp(void) {}
void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    test_hash_functions_suite();
    test_data_node_suite();
    // Add more test suites here as needed
    return UNITY_END();
}
