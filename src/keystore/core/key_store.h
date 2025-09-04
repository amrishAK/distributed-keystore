#ifndef KEY_STORE_H
#define KEY_STORE_H

#include "type_definition.h"

int initialise_key_store(int bucket_size);
int set_key(const char *key, const unsigned char *data, size_t data_size);
data_node* get_key(const char *key);
int delete_key(const char *key);


#endif // KEY_STORE_H