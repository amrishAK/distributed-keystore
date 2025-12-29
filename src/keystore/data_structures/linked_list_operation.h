#ifndef LINKED_LIST_OPERATIONS_H
#define LINKED_LIST_OPERATIONS_H

#include "type_definitions/hash_bucket_type_definition.h"
#include "type_definitions/error_code_definitions.h"

typedef enum{
    CREATE_LL_NODE,
    INSERT_LL_NODE,
    GET_LL_NODE,
    DELETE_ALL_LL_NODES,
    CLEANUP_DELETED_LL_NODES
} linked_list_node_operation_t;

/**
 * @fn create_new_linked_list_node
 * @brief Creates a new linked list node with the specified key hash and data.
 *
 * This function allocates memory for a new list_node, initializes it with the provided
 * key hash and data node, and sets the next pointer to NULL.
 *
 * @param key_hash The hash value of the key to be stored in the new node.
 * @param data Pointer to the data_node to be associated with the new list node.
 * @param new_list_node_out Pointer to receive the newly created list_node.
 * @return int Returns 0 on success, or a negative value on failure.
 * @note This operation does not require to be passed via locking wrapper,
 *  a lock will be acquired during memory allocation if concurrency is enabled.
 */
int create_new_linked_list_node(uint32_t key_hash, data_node *data, linked_list_node **new_list_node_out);

/**
 * @fn insert_linked_list_node
 * @brief Inserts a new node into the linked list.
 * 
 * This function insserts a new list_node at the head of the linked list
 *
 * @param node_header_ptr Pointer to the linked list head.
 * @param new_list_node Pointer to the newly created list_node to be inserted.
 * @note Use create_new_linked_list_node to create the new node. Should dispose it if insertion fails.
 * @return int Returns 0 on success, or a negative value on failure.
 */
int insert_linked_list_node(linked_list_node **node_header_ptr, linked_list_node* new_list_node);

/**
 * @fn get_data_node_from_linked_list
 * @brief Retrieves a data node from the linked list based on the provided key and key hash.
 * This function searches for a node in the list whose key matches the given key and key_hash,
 * and returns its associated data_node.
 * @param node_header_ptr Pointer to the head of the linked list.
 * @param key The key to search for in the list.
 * @param key_hash The hash value of the key to optimize search.
 * @param data_node_out Pointer to a data_node pointer to receive the found node's data.
 * @return int Returns 0 on success, or a non-zero value if the node was not found or error occurs. 
 **/
int get_data_node_from_linked_list(linked_list_node *node_header_ptr, const char *key, uint32_t key_hash, data_node **data_node_out);


/**
 * @fn delete_all_linked_list_nodes
 * @brief Deletes all nodes in the linked list starting from the given head pointer.
 *
 * This function traverses the linked list, deletes each data_node associated with
 * the list_node, and frees the memory allocated for each list_node.
 *
 * @param node_header_ptr Pointer to the head of the linked list.
 * @return int Returns 0 on success.
 */
int delete_all_linked_list_nodes(linked_list_node *node_header_ptr);


/**
 * @fn cleanup_deleted_linked_list_nodes
 * @brief Cleans up and frees all nodes in the linked list that have been marked as deleted.
 *
 * This function traverses the linked list starting from the given head pointer,
 * identifies nodes that are marked as deleted, deletes their associated data_node,
 * frees the memory allocated for each deleted list_node, and updates the count of deleted nodes.
 *
 * @param node_header_ptr Pointer to the head of the linked list.
 * @param deleted_count_out Pointer to an integer to receive the count of deleted nodes.
 * @return int Returns deleted node count on success. or a negative value on failure.
 */
int cleanup_deleted_linked_list_nodes(linked_list_node *node_header_ptr);

#endif // LINKED_LIST_OPERATIONS_H