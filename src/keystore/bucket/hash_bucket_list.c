#include <string.h>
#include <stdlib.h>
#include "hash_bucket_list.h"
#include "core/data_node.h"


list_node* create_new_list_node(uint32_t key_hash, data_node *data);
bool list_node_hash_equals(list_node *node, uint32_t key_hash, const char *key);

list_node *insert_list_node(list_node *node_header_ptr, uint32_t key_hash, data_node *data)
{
    // Argument validation
    if(key_hash < 0 || data == NULL)
    {
        return NULL; // Handle invalid parameters
    }

    list_node *new_node = create_new_list_node(key_hash, data);

    if(node_header_ptr == NULL) {
        return new_node;
    } else {
        new_node->next = node_header_ptr;
        return new_node;
    }
}

int delete_list_node(list_node *node_header_ptr, const char *key, uint32_t key_hash)
{
    // Argument validation
    if(key == NULL || key_hash < 0 || node_header_ptr == NULL)
    {
        return -1; // Validation failed
    }

    list_node *current_node_ptr = node_header_ptr;
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
        return -1; // Node not found
    }

    // Node found, perform deletion
    if(previous_node_ptr == NULL)
    {
        node_header_ptr = current_node_ptr->next;
    }
    else
    {
        previous_node_ptr->next = current_node_ptr->next;
    }

    // Free the memory allocated for the node
    delete_data_node(current_node_ptr->data);
    free(current_node_ptr);

    return 0; // Success

}

list_node *find_list_node(list_node *node_header_ptr, const char *key, uint32_t key_hash)
{
    // Validate input parameters
    if(node_header_ptr == NULL || key == NULL || key_hash == 0)
    {
        return NULL;
    }

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

list_node* create_new_list_node(uint32_t key_hash, data_node *data)
{
    list_node *new_node = (list_node *)malloc(sizeof(list_node));
    
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
