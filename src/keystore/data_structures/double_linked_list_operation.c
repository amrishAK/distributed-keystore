#include "double_linked_list_operation.h"
#include "utils/memory_manager.h"
#include "data_node_operation.h"

#include <string.h>

#pragma region Private Function Definitions
bool _list_node_hash_equals(double_linked_list_node *node_ptr, composite_key_hash key_hash, const char *key, bool include_soft_deleted);
#pragma endregion

int create_new_double_linked_list_node(composite_key_hash key_hash, data_node* data_node_ptr, double_linked_list_node** new_node_out)
{
    if (data_node_ptr == NULL || new_node_out == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input

    double_linked_list_node* new_node = (double_linked_list_node*)allocate_memory(sizeof(double_linked_list_node));
    if (new_node == NULL) return ERR_MEMORY_ALLOCATION_FAILED; // Error handling: memory allocation failure

    new_node->key_hash = key_hash.sub_bucket_hash; // Use sub bucket hash for the double linked list node
    new_node->data_node_ptr = data_node_ptr;
    new_node->prev_node_ptr = NULL;
    new_node->next_node_ptr = NULL;

    *new_node_out = new_node;
    return SUCCESS;
}


int insert_double_linked_list_node(double_linked_list_node** head_ptr, double_linked_list_node* new_node)
{
    if (head_ptr == NULL || new_node == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input

    new_node->next_node_ptr = *head_ptr;
    new_node->prev_node_ptr = NULL;

    if (*head_ptr != NULL) {
        (*head_ptr)->prev_node_ptr = new_node;
    }

    *head_ptr = new_node;
    return SUCCESS;
}


int find_data_node_in_double_linked_list(double_linked_list_node* head_ptr, const char* key, composite_key_hash key_hash, bool include_deleted, data_node** data_node_out)
{
    if (key == NULL || data_node_out == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input

    double_linked_list_node* current_node_ptr = head_ptr;

    while (current_node_ptr != NULL)
    {
        if(_list_node_hash_equals(current_node_ptr, key_hash, key, include_deleted))
        {
                *data_node_out = current_node_ptr->data_node_ptr;
            return SUCCESS; // Success
        }

        current_node_ptr = current_node_ptr->next_node_ptr;
    }

    return ERR_DATA_NODE_NOT_FOUND; // Node with specified key and hash not found
}


int delete_double_linked_list(double_linked_list_node* head_ptr)
{
    if(head_ptr == NULL) return SUCCESS; // Nothing to delete

    double_linked_list_node* current_node = head_ptr;
    while (current_node != NULL) {
        double_linked_list_node* next_node = current_node->next_node_ptr;
        delete_data_node(current_node->data_node_ptr);
        free_memory(current_node, false);
        current_node = next_node;
    }

    return SUCCESS;
}


#pragma region Private Helper Functions
/**
 * @fn _list_node_hash_equals
 * @brief Compares the key hash and key of a double linked list node with the provided values.
 *
 * This function checks if the double linked list node's key hash matches the provided key hash,
 * and if so, compares the actual keys for equality.
 *
 * @param node_ptr Pointer to the double linked list node to compare.
 * @param key_hash Composite hash value of the key to compare, The composite key contains both bucket hash and sub bucket hash of the key.
 * @param key The key string to compare against.
 * @return bool Returns true if both the key hash and key match; otherwise, false.
 * @note If the data node is marked as deleted, the function returns false.
 */
bool _list_node_hash_equals(double_linked_list_node *node_ptr, composite_key_hash key_hash, const char *key, bool include_soft_deleted)
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