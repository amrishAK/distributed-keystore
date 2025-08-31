#ifndef DATA_NODE_H
#define DATA_NODE_H

#include <stdint.h>
#include "type_definition.h"

data_node* create_data_node(const char *key, uint32_t key_hash, const unsigned char *data, size_t data_size);
int update_data_node(data_node *node, const unsigned char *data, size_t data_size);
int delete_data_node(data_node *node);

#endif // DATA_NODE_H