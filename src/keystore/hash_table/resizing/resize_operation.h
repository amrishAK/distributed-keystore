#ifndef RESIZE_OPERATION_H
#define RESIZE_OPERATION_H


#include "type_definitions/error_code_definitions.h"
#include "type_definitions/sucess_code_definitions.h"
#include "type_definitions/hash_bucket_type_definition.h"
#include "type_definitions/custom_type_definitions.h"

/**  
 * @fn hash_bucket_resize_worker
 * @brief Worker function to handle resizing of a hash bucket.
 *
 * This function is intended to be run in a separate thread to perform the resizing
 * operation on a hash bucket. It manages the transfer of data from the old sub-hash-table
 * to the new one and ensures consistency during the resizing process.
 *
 * @param input_arg Pointer to the arguments required for the resize operation.
 * @return 0 on success, or a negative error code on failure.
 */
int hash_bucket_resize_worker(void* input_arg);


#endif // RESIZE_OPERATION_H