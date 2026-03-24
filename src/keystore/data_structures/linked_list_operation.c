#include "linked_list_operation.h"
#include "data_node_operation.h"
#include "utils/memory_manager.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>


#pragma region Private Helper Functions
static bool _list_node_hash_equals(linked_list_node *node_ptr, composite_key_hash key_hash, const char *key, bool include_soft_deleted);
#pragma endregion

#pragma region Private Function Declarations
static int _create_new_linked_list_node(composite_key_hash key_hash, data_node *data, linked_list_node **new_list_node_out);
static int _insert_linked_list_node(linked_list_node **node_header_ptr, linked_list_node* new_list_node);
static int _get_data_node_from_linked_list(linked_list_node *node_header_ptr, const char *key, composite_key_hash key_hash, bool include_soft_deleted, data_node **data_node_out);
static int _delete_all_linked_list_nodes(linked_list_node **node_header_ptr);
static int _cleanup_deleted_linked_list_nodes(linked_list_node **node_header_ptr);
#pragma endregion

#pragma region Public Function Definitions
int create_new_linked_list_node(composite_key_hash key_hash, data_node *data, linked_list_node **new_list_node_out)
{
    int result = _create_new_linked_list_node(key_hash, data, new_list_node_out);
    return result;
}

int insert_linked_list_node(linked_list_node **node_header_ptr, linked_list_node* new_list_node)
{
    int result = _insert_linked_list_node(node_header_ptr, new_list_node);
    return result;
}

int get_data_node_from_linked_list(linked_list_node *node_header_ptr, const char *key, composite_key_hash key_hash, bool include_soft_deleted, data_node **data_node_out)
{
    int result = _get_data_node_from_linked_list(node_header_ptr, key, key_hash, include_soft_deleted, data_node_out);
    return result;
}

int delete_all_linked_list_nodes(linked_list_node **node_header_ptr)
{
    int result = _delete_all_linked_list_nodes(node_header_ptr);
    return result;
}

int cleanup_deleted_linked_list_nodes(linked_list_node **node_header_ptr)
{
    int result = _cleanup_deleted_linked_list_nodes(node_header_ptr);
    return result;
}

#pragma endregion

#pragma region Private Function Definitions

int _create_new_linked_list_node(composite_key_hash key_hash, data_node *data, linked_list_node **new_list_node_out)
{
    if (data == NULL || new_list_node_out == NULL) {
        return ERR_INVALID_ARGUMENT; // Invalid data or output pointer
    }

    linked_list_node *new_node = allocate_memory_from_pool();
    if (new_node == NULL) {
        return ERR_MEMORY_ALLOCATION_FAILED; // Memory allocation failure
    }

    new_node->key_hash = key_hash.sub_bucket_hash; // Store only sub bucket hash in the linked list node
    new_node->data_node_ptr = data;
    new_node->next_node_ptr = NULL;

    *new_list_node_out = new_node;
    return 0;
}

int _insert_linked_list_node(linked_list_node **node_header_ptr, linked_list_node* new_list_node)
{
    if(new_list_node == NULL || node_header_ptr == NULL) {
        return ERR_INVALID_ARGUMENT; // Invalid data or key
    }

    if(*node_header_ptr != NULL)
    {
        new_list_node->next_node_ptr = *node_header_ptr;
    }
    *node_header_ptr = new_list_node;
    
    return 0;
}


int _get_data_node_from_linked_list(linked_list_node *node_header_ptr, const char *key, composite_key_hash key_hash, bool include_soft_deleted, data_node **data_node_out)
{
    if (key == NULL || data_node_out == NULL) return ERR_INVALID_ARGUMENT; // Invalid parameters

    linked_list_node *current_node_ptr = node_header_ptr;
    while (current_node_ptr != NULL)
    {
        if(_list_node_hash_equals(current_node_ptr, key_hash, key, include_soft_deleted))
        {
            *data_node_out = current_node_ptr->data_node_ptr;
            return 0; // Success
        }

        current_node_ptr = current_node_ptr->next_node_ptr;
    }

    return ERR_DATA_NODE_NOT_FOUND; // Node with specified key and hash not found
}


int _delete_all_linked_list_nodes(linked_list_node **node_header_ptr)
{
    if(node_header_ptr == NULL || *node_header_ptr == NULL) return SUCCESS; // Nothing to delete

    linked_list_node *current_node_ptr = *node_header_ptr;
    linked_list_node *next_node_ptr = NULL;

    while (current_node_ptr != NULL)
    {
        next_node_ptr = current_node_ptr->next_node_ptr;
        delete_data_node(current_node_ptr->data_node_ptr);
        free_memory(current_node_ptr, true);
        current_node_ptr = next_node_ptr;
    }

    *node_header_ptr = NULL; // Set the head pointer to NULL after deletion
    return SUCCESS; // Success
}

int _cleanup_deleted_linked_list_nodes(linked_list_node **node_header_ptr)
{
    // If the list is empty, set deleted count to 0 and return success
    if (node_header_ptr == NULL || *node_header_ptr == NULL) return SUCCESS;

    int deleted_count = 0;
    int delete_result = 0;

    linked_list_node *current_node_ptr = *node_header_ptr;
    linked_list_node *prev_node_ptr = NULL;
    linked_list_node *next_node_ptr = NULL;

    while (current_node_ptr != NULL)
    {
        next_node_ptr = current_node_ptr->next_node_ptr;

        if (current_node_ptr->data_node_ptr->is_deleted)
        {
            //Check if the node to be deleted is the head node
            if (prev_node_ptr == NULL)
            {
                *node_header_ptr = next_node_ptr; // Update head pointer if head node is deleted
            }
            else
            {
                prev_node_ptr->next_node_ptr = next_node_ptr; // Bypass the deleted node
            }

            // Delete the data node and free the list node
            delete_result = delete_data_node(current_node_ptr->data_node_ptr);
            if(delete_result != SUCCESS) return delete_result; // Return if data node deletion fails
            free_memory(current_node_ptr, true);
            deleted_count++;
        }
        else
        {
            prev_node_ptr = current_node_ptr;
        }
        
        current_node_ptr = next_node_ptr;
    }
    
    // Return the count of deleted nodes 
    //(non-negative value indicates success, and also provides the count of deleted nodes)

    return deleted_count;     
}

#pragma endregion


#pragma region Private Helper Functions


/**
 * @fn _list_node_hash_equals
 * @brief Compares the key hash and key of a linked list node with the provided values.
 *
 * This function checks if the linked list node's key hash matches the provided key hash,
 * and if so, compares the actual keys for equality.
 *
 * @param node Pointer to the linked list node to compare.
 * @param key_hash Composite hash value of the key to compare, The composite key contains both bucket hash and sub bucket hash of the key.
 * @param key The key string to compare against.
 * @return bool Returns true if both the key hash and key match; otherwise, false.
 * @note If the data node is marked as deleted, the function returns false.
 */
bool _list_node_hash_equals(linked_list_node *node_ptr, composite_key_hash key_hash, const char *key, bool include_soft_deleted)
{
    bool result = false;

    if(node_ptr->data_node_ptr->is_deleted && !include_soft_deleted) return false;
    if(node_ptr->key_hash == key_hash.sub_bucket_hash)
    {
        result = (strcmp(node_ptr->data_node_ptr->key, key) == 0);
    }

    return result;
}

#pragma endregion