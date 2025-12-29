#include "unity.h"
#include "test_data_structures/test_data_node_operation.c"
#include "test_data_structures/test_linked_list_operation.c"
#include "test_sub_hash_tables/test_sub_hash_table_operation.c"
#include "test_sub_hash_tables/test_sub_hash_bucker_operation.c"
#include "test_hash_tables/test_hash_bucket_operation.c"
#include "test_hash_tables/test_hash_table_operation.c"

void setUp(void) {}
void tearDown(void) {}

int main(void) {
    int result = 0;
    result = test_data_node_operations_main();
    result = test_linked_list_operations_main();
    result = test_sub_hash_bucket_operation_main();
    result = test_sub_hash_table_operation_main();
    result = test_hash_bucket_operation_main();
    result = test_hash_table_operation_main();
    return result;
}


