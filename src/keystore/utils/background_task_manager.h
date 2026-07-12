#ifndef BACKGROUND_TASK_MANAGER_H
#define BACKGROUND_TASK_MANAGER_H

#include <stdint.h>
#include <stddef.h>
#include "type_definitions/error_code_definitions.h"
#include "type_definitions/sucess_code_definitions.h"
#include "type_definitions/background_task_manager_type_definitons.h"
#include <stdbool.h>

/**
 * @fn initialize_background_function
 * @brief Initializes and starts a background function in a separate thread with a unique task identifier.
 * @param background_task Pointer to the background function to be executed. (The function should take a void* argument and return an int.)
 * @param task_args Pointer to the arguments to be passed to the background function.
 * @param detach_task Boolean flag indicating whether the background task should be detached. 
 * If true, the background task will run independently and cannot be joined; if false, the caller can wait for the task to complete using its unique identifier.
 * @param task_uuid_out Pointer variable with uuid returned to the caller to manage the background task. 
 * If detach_task is true, uuid will be set to 0
 * This uuid can be used to track, wait for completion, or clean up the background task.
 * @return int Returns 0 on success, or a negative value on failure. 
 * On success, the background task will be running and can be managed using the provided task_uuid.
 */
int initialize_background_function(int (*background_task)(void*), void* task_args, bool detach_task, uint32_t* task_uuid_out);

/**
 * @fn cleanup_background_task
 * @brief Waits for a background task to complete and cleans up resources using its unique identifier.
 * This function should be called to wait for a background task to complete and free resources associated with it.
 * @param task_uuid The unique identifier of the background task to be cleaned up.
 * @return int Returns 0 on success, or a negative value on failure. 
 * On success, resources associated with the background task will be freed and any necessary cleanup will be performed.
 */
int cleanup_background_task(uint32_t task_uuid);

#endif // BACKGROUND_TASK_MANAGER_H