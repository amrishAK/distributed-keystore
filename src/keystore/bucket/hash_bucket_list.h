#ifndef HASH_BUCKET_LIST_H
#define HASH_BUCKET_LIST_H

#include "core/type_definition.h"
#include <stdbool.h>

/// @brief Create a new list node.
/// @param node_header_ptr Pointer to the head node of the linked list.
/// @param key The key for the new node.
/// @param data The data for the new node.
/// @param data_size The size of the data for the new node.
/// @return Pointer to the newly created list node.
int insert_list_node(list_node **node_header_ptr, uint32_t key_hash, data_node *data);

/// @brief Delete a list node.
/// @param node_header_ptr Pointer to the head node of the linked list.
/// @param key The key of the node to delete.
/// @param key_hash The hash of the key to delete.
int delete_list_node(list_node **node_header_ptr, const char *key, uint32_t key_hash);

/// @brief Find a list node by key and key hash.
/// @param node_header_ptr Pointer to the head node of the linked list.
/// @param key The key to search for.
/// @param key_hash The hash of the key to search for.
/// @return Pointer to the found list node, or NULL if not found.
list_node* find_list_node(list_node *node_header_ptr, const char *key, uint32_t key_hash);

int delete_all_list_nodes(list_node *node_header_ptr);

#endif // HASH_BUCKET_LIST_H