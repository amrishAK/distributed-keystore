#ifndef HELPER_FUNCTIONS_H
#define HELPER_FUNCTIONS_H

#include <stdbool.h>

/**
 * @fn is_power_of_two
 * @brief Checks if a given unsigned integer is a power of two.
 * @param bucket_size The unsigned integer to check.
 * @return int Returns 1 (true) if the number is a power of two, otherwise returns 0 (false).
 */
int is_power_of_two(unsigned int bucket_size);


/**
 * @fn initialize_background_function
 * @brief Initializes and starts a background function in a separate thread.
 * @param background_task Pointer to the background function to be executed. (The function should take a void* argument and return an int.)
 * @param task_args Pointer to the arguments to be passed to the background function.
 * @return int Returns 0 on success, or a negative value on failure.
 */
int initialize_background_function(int (background_task)(void*), void* task_args);


#endif // HELPER_FUNCTIONS_H