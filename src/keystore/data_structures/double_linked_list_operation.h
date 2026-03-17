#ifndef DOUBLE_LINKED_LIST_OPERATION_H
#define DOUBLE_LINKED_LIST_OPERATION_H

#include "type_definitions/custom_type_definitions.h"
#include "type_definitions/error_code_definitions.h"
#include "type_definitions/sucess_code_definitions.h"

/**
 * @brief Creates a new double linked list node.
 * @param key_hash Hash of the key.
 * @param data_node_ptr Pointer to the associated data node.
 * @param new_node_out Output pointer to the newly created node.
 * @return Status code.
 */
int create_new_double_linked_list_node(uint32_t key_hash, data_node* data_node_ptr, double_linked_list_node** new_node_out);

/**
 * @brief Inserts a node at the head of the double linked list.
 * @param head_ptr Pointer to the head pointer of the list.
 * @param new_node Node to insert.
 * @return Status code.
 */
int insert_double_linked_list_node(double_linked_list_node** head_ptr, double_linked_list_node* new_node);

/**
 * @brief Finds a data node in the double linked list.
 * @param head_ptr Pointer to the head of the list.
 * @param key Key to search for.
 * @param key_hash Hash of the key.
 * @param include_deleted Whether to include deleted nodes in the search.
 * @param data_node_out Output pointer to the found data node.
 * @return Status code.
 */
int find_data_node_in_double_linked_list(double_linked_list_node* head_ptr, const char* key, uint32_t key_hash, bool include_deleted, data_node** data_node_out);

/**
 * @brief Deletes the entire double linked list.
 * @param head_ptr Pointer to the head of the list.
 * @return Status code.
 */
int delete_double_linked_list(double_linked_list_node* head_ptr);

#endif // DOUBLE_LINKED_LIST_OPERATION_H