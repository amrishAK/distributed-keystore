#ifndef HASH_BUCKET_LIST_H
#define HASH_BUCKET_LIST_H

#include "core/type_definition.h"
#include <stdbool.h>


/**
 * @fn insert_list_node
 * @brief Inserts a new node into the linked list.
 *
 * This function creates a new list node with the specified key hash and associated data,
 * and inserts it at the appropriate position in the list pointed to by node_header_ptr.
 *
 * @param node_header_ptr Pointer to the head pointer of the linked list.
 * @param key_hash Hash value of the key to be stored in the new node.
 * @param data Pointer to the data node to be associated with the new list node.
 * @return int Returns 0 on success, or a negative value on failure.
 */
int insert_list_node(list_node **node_header_ptr, uint32_t key_hash, data_node *data);

/**
 * @fn delete_list_node
 * @brief Deletes a node from the linked list based on the provided key and key hash.
 *
 * This function searches for a node in the list whose key matches the given key and key_hash,
 * removes it from the list, and frees its memory.
 *
 * @param node_header_ptr Pointer to the pointer of the list's head node.
 * @param key The key to search for in the list.
 * @param key_hash The hash value of the key to optimize search.
 * @return int Returns 0 on successful deletion, or a non-zero value if the node was not found.
 */

int delete_list_node(list_node **node_header_ptr, const char *key, uint32_t key_hash);


/**
 * @fn find_list_node
 * @brief Finds a node in a linked list matching the specified key and key hash.
 *
 * Traverses the linked list starting from the given node header pointer, searching for a node
 * whose key and key hash match the provided values.
 *
 * @param node_header_ptr Pointer to the head of the linked list.
 * @param key The key to search for in the list.
 * @param key_hash The hash value of the key to match.
 * @return Pointer to the matching list_node if found, otherwise NULL.
 */
list_node* find_list_node(list_node *node_header_ptr, const char *key, uint32_t key_hash);

/**
 * @fn delete_all_list_nodes
 * @brief Deletes all nodes in the linked list starting from the given node header pointer.
 *
 * This function iterates through the linked list, deleting each node and freeing its associated
 * data until the entire list is cleared.
 *
 * @param node_header_ptr Pointer to the head of the linked list to be deleted.
 * @return int Returns 0 on success, or a negative value if an error occurs.
 */
int delete_all_list_nodes(list_node *node_header_ptr);

#endif // HASH_BUCKET_LIST_H