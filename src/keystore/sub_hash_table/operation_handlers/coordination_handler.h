#ifndef COORDINATION_HANDLER_H
#define COORDINATION_HANDLER_H

#include "type_definitions/custom_type_definitions.h"


/**
 * @fn find_data_node
 * @brief Finds a data node in the sub-hash-bucket by key and composite key hash.
 * @param args Structure containing sub-hash-bucket pointer, key, and composite key hash.
 * @param data_node_out Pointer to a data_node pointer to receive the found node.
 * @return int Returns 0 on success, or a negative value if the node was not found.
 */
int find_data_node(const sub_hash_bucket_operation_args args, data_node** data_node_out);

/**
 * @fn add_data_node
 * @brief Adds a new data node to the sub-hash-bucket by key and composite key hash.
 * @param args Structure containing sub-hash-bucket pointer, key, and composite key hash.
 * @param new_node Pointer to the new data node to add.
 * @return int Returns 0 on success, or a negative value on error.
 */
int add_data_node(const sub_hash_bucket_operation_args args, data_node* new_node);


/**
 * @fn delete_all_data_nodes
 * @brief Deletes all data nodes in the sub-hash-bucket by key and composite key hash.
 * @param args Structure containing sub-hash-bucket pointer, key, and composite key hash.
 * @return int Returns 0 on success, or a negative value if an error occurs.
 * @note This function performs a hard delete, meaning all nodes are physically removed and memory is freed.
 */
int delete_all_data_nodes(const sub_hash_bucket_operation_args args);

/**
 * @fn cleanup_deleted_data_nodes
 * @brief Cleans up soft deleted data nodes in the sub-hash-bucket by key and composite key hash.
 * @param args Structure containing sub-hash-bucket pointer, key, and composite key hash.
 * @param deleted_count_out Pointer to receive the count of nodes that were cleaned up.
 * @return int Returns 0 on success, or a negative value if an error occurs.
 * @note This function performs a hard delete of nodes that were previously soft deleted, meaning the nodes are physically removed and memory is freed.
 */
int cleanup_deleted_data_nodes(const sub_hash_bucket_operation_args args, unsigned int* deleted_count_out);


#endif // COORDINATION_HANDLER_H
