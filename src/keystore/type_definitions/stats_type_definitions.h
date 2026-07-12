#ifndef STATS_TYPE_DEFINITIONS_H
#define STATS_TYPE_DEFINITIONS_H

typedef struct{
    unsigned long successful_read_operations;
    unsigned long successful_create_operations;
    unsigned long successful_update_operations;
    unsigned long successful_delete_operations;
    unsigned long successful_soft_delete_operations;
    unsigned long failed_read_operations;
    unsigned long failed_create_operations;
    unsigned long failed_update_operations;
    unsigned long failed_delete_operations;
    unsigned long failed_soft_delete_operations;

    unsigned long error_code_counters[100]; // Index corresponds to negative error codes
} data_node_operation_stats;


#endif // STATS_TYPE_DEFINITIONS_H