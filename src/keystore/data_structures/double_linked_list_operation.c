#include "double_linked_list_operation.h"
#include "utils/memory_manager.h"
#include "data_node_operation.h"

#include <string.h>


int create_new_double_linked_list_node(uint32_t key_hash, data_node* data_node_ptr, double_linked_list_node** new_node_out)
{
    if (data_node_ptr == NULL || new_node_out == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input

    double_linked_list_node* new_node = (double_linked_list_node*)allocate_memory(sizeof(double_linked_list_node));
    if (new_node == NULL) return ERR_MEMORY_ALLOCATION_FAILED; // Error handling: memory allocation failure

    new_node->key_hash = key_hash;
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


int find_data_node_in_double_linked_list(double_linked_list_node* head_ptr, const char* key, uint32_t key_hash, bool include_deleted, data_node** data_node_out)
{
    if (key == NULL || data_node_out == NULL) return ERR_INVALID_ARGUMENT; // Error handling: invalid input

    double_linked_list_node* current_node = head_ptr;

    while (current_node != NULL) {
        if (current_node->key_hash == key_hash) {
            data_node* candidate_data_node = current_node->data_node_ptr;
            if (strcmp(candidate_data_node->key, key) == 0) {
                if (!include_deleted && candidate_data_node->is_deleted) {
                    return ERR_DATA_NODE_NOT_FOUND; // Node is marked as deleted
                }
                *data_node_out = candidate_data_node;
                return SUCCESS; // Node found
            }
        }
        current_node = current_node->next_node_ptr;
    }

    return ERR_DATA_NODE_NOT_FOUND; // Node not found
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