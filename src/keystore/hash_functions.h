#ifndef HASH_FUNCTIONS_H
#define HASH_FUNCTIONS_H

#include <stdint.h>
#include <stddef.h>

uint32_t hash_function_murmur_32(const char *key,  uint32_t seed);

#endif // HASH_FUNCTIONS_H