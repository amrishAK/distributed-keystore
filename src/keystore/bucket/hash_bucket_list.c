#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "hash_bucket_list.h"
#include "core/data_node.h"
#include "utils/memory_manager.h"


list_node* create_new_list_node(uint32_t key_hash, data_node *data);
bool list_node_hash_equals(list_node *node, uint32_t key_hash, const char *key);

int insert_list_node(list_node **node_header_ptr, list_node* new_list_node)
{
    if(new_list_node == NULL) {
        return -20; // Invalid data or key
    }

    if(node_header_ptr != NULL)
    {
        new_list_node->next = *node_header_ptr;
    }

    *node_header_ptr = new_list_node;
    return 0;
}

int delete_list_node(list_node **node_header_ptr, const char *key, uint32_t key_hash, data_node **deleted_node_out)
{
    if (!node_header_ptr || !*node_header_ptr || !key || !deleted_node_out) return -21;

    list_node *current_node_ptr = *node_header_ptr;
    list_node *previous_node_ptr = NULL;
    bool node_found = false;

    while (current_node_ptr != NULL)
    {
        if(list_node_hash_equals(current_node_ptr, key_hash, key))
        {
            node_found = true;
            break;
        }

        previous_node_ptr = current_node_ptr;
        current_node_ptr = current_node_ptr->next;
    }

    if(!node_found)
    {
        return -41; // Node with specified key and hash not found
    }

    // Node found, perform deletion
    if(previous_node_ptr == NULL)
    {
        *node_header_ptr = current_node_ptr->next;
    }
    else
    {
        previous_node_ptr->next = current_node_ptr->next;
    }

    
    *deleted_node_out = current_node_ptr->data;

    // Free the list node structure but not the data node
    free_memory(current_node_ptr, LIST_POOL);

    return 0; // Success
}

list_node *find_list_node(list_node *node_header_ptr, const char *key, uint32_t key_hash)
{
    list_node *found_node = NULL;

    list_node *current_node_ptr = node_header_ptr;

    while (current_node_ptr != NULL)
    {
        if(list_node_hash_equals(current_node_ptr, key_hash, key))
        {
            found_node = current_node_ptr;
            break;
        }

        current_node_ptr = current_node_ptr->next;
    }

    return found_node;
}

int delete_all_list_nodes(list_node *node_header_ptr)
{
    if(node_header_ptr == NULL) {
        return 0;
    }

    list_node *current_node_ptr = node_header_ptr;
    list_node *next_node_ptr = NULL;

    while (current_node_ptr != NULL)
    {
        next_node_ptr = current_node_ptr->next;
        delete_data_node(current_node_ptr->data);
        free_memory(current_node_ptr, LIST_POOL);
        current_node_ptr = next_node_ptr;
    }

    return 0; // Success
}

list_node* create_new_list_node(uint32_t key_hash, data_node *data)
{
    list_node *new_node = (list_node *)allocate_memory_from_pool(LIST_POOL);

    if (new_node == NULL) {
        return NULL; // Handle memory allocation failure
    }

    new_node->key_hash = key_hash;
    new_node->data = data;
    new_node->next = NULL;

    return new_node;
}

bool list_node_hash_equals(list_node *node, uint32_t key_hash, const char *key)
{
    bool result = false;

    if(node->key_hash == key_hash)
    {
        result = (strcmp(node->data->key, key) == 0);
    }

    return result;
}
