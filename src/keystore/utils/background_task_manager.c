#include "background_task_manager.h"
#include "memory_manager.h"

#pragma region  Private Type Definitions

#pragma endregion

#pragma region global variables for background task management
// In-memory registry to track background tasks by their unique identifiers
#define MAX_BACKGROUND_TASKS 100
background_task_info_t* background_tasks_registry[MAX_BACKGROUND_TASKS];
int initialised_items_count = 0;
int active_tasks_count = 0;
pthread_mutex_t registry_lock = PTHREAD_MUTEX_INITIALIZER;
#pragma endregion

#pragma region Private Registry Function Declarations
static int _register_background_task(background_task_t* task_info, pthread_t thread_id, uint32_t* out_task_uuid);
static int _add_background_task_to_registry(background_task_info_t* task_info);
static int _get_background_task_info(uint32_t task_uuid, background_task_info_t** out_task_info);
static int _remove_background_task_from_registry(uint32_t task_uuid);
#pragma endregion

#pragma region Functions for POSIX Platform Declarations
static void* _background_task_wrapper(void* vp);
static int _initialize_background_function_pthread(background_task_t* task_info, pthread_t* out_thread_id);
static int _generate_unique_task_uuid(uint32_t* out_uuid);
#pragma endregion


#pragma region Public Function Definitions

int initialize_background_function(int (*background_task)(void*), void* task_args, bool detach_task, uint32_t* task_uuid_out) {
    
    if(background_task == NULL || task_args == NULL ) return ERR_INVALID_ARGUMENT;
    if(!detach_task && task_uuid_out == NULL) return ERR_INVALID_ARGUMENT; // If the task is not detached, we need to return a uuid for management

    int result = SUCCESS;
    pthread_t thread_id;
    uint32_t task_uuid;

    // Initialize the background function using pthreads
    background_task_args_t* args = allocate_memory(sizeof(background_task_args_t));
    if (!args) return ERR_MEMORY_ALLOCATION_FAILED;
    args->task_input_args = task_args;
    atomic_store(&args->kill_signal, false); // Initialize kill signal to false

    background_task_t* task = allocate_memory(sizeof(background_task_t));
    if (!task) {
        free(args);
        return ERR_MEMORY_ALLOCATION_FAILED;
    }
    task->task_args = args;
    task->background_task = background_task;
    task->is_detached = detach_task;
    
    result = _initialize_background_function_pthread(task, &thread_id);
    if (result != 0) return result;

    if(detach_task) {
        pthread_detach(thread_id);
        task_uuid = 0; // Indicate that the task is detached and does not have a UUID for management
    }
    else{
        result = _register_background_task(task, thread_id, &task_uuid);
        if (result != 0) return result;
        *task_uuid_out = task_uuid;
    }

    return result;
}

int cleanup_background_task(uint32_t task_uuid) {
    if(task_uuid == UINT32_MAX) return ERR_INVALID_ARGUMENT;

    background_task_info_t* task_info;
    int result = _get_background_task_info(task_uuid, &task_info);
    if (result != SUCCESS) return result;

    // Signal the background task to stop by setting the kill signal
    atomic_store(&task_info->task->task_args->kill_signal, true);
    pthread_join(task_info->pthread_id, NULL); // Wait for the background task to finish

    // Remove the task from the registry
    result = _remove_background_task_from_registry(task_uuid);

    return result;
}

#pragma endregion


#pragma region Private Function Definitions

/**
 * @fn _generate_unique_task_uuid
 * @brief Generates a unique identifier for a background task. 
 * This function uses an atomic counter to ensure thread-safe generation of unique UUIDs for background tasks.
 * @param out_uuid Pointer to a uint32_t variable where the generated unique identifier will be stored.
 * @return SUCCESS on success, or an error code on failure.
 */
int _generate_unique_task_uuid(uint32_t* out_uuid) {
    if (out_uuid == NULL) return ERR_INVALID_ARGUMENT;
    static uint32_t current_uuid = 1; // Start UUIDs from 1, as 0 can be used to indicate detached tasks
    uint32_t uuid = __sync_fetch_and_add(&current_uuid, 1); // Increment UUID
    // Ensure uuid is never 0 (should not happen, but just in case)
    if (uuid == 0) uuid = __sync_fetch_and_add(&current_uuid, 1);
    *out_uuid = uuid;
    return SUCCESS;
}

/**
 * @fn _background_task_wrapper
 * @brief Wrapper function for executing a background task.
 * This function is used as the entry point for a new thread. It checks the kill signal before starting the task
 * and ensures proper cleanup of resources if the task is detached.
 * @param vp Pointer to the background_task_t structure containing task information.
 * @return NULL
 */
static void* _background_task_wrapper(void* vp) {
    if(vp == NULL) return NULL;

    background_task_t* args = (background_task_t*)vp;

    if (atomic_load(&args->task_args->kill_signal) == false) // Check if kill signal is not set before starting the task
    {
        atomic_store(&args->task_args->kill_signal, false); // Ensure kill signal is reset before starting the task
        args->background_task(args->task_args);
    }

    if (args->is_detached)
    {
        free_memory(args->task_args, false);
        free_memory(args, false);
    }
    return NULL;
}

/**
 * @fn _initialize_background_function_pthread
 * @brief Initializes a background function using POSIX threads.
 * This function creates a new thread to execute the provided background task and returns the thread ID.
 * @param task_info Pointer to the background_task_t structure containing task information.
 * @param out_thread_id Pointer to a pthread_t variable where the created thread ID will be stored.
 * @return SUCCESS on success, or an error code on failure.
 */
int _initialize_background_function_pthread(background_task_t* task_info, pthread_t* out_thread_id) {
    
    if(task_info == NULL || out_thread_id == NULL) return ERR_INVALID_ARGUMENT;

    pthread_t thread_id;
    int create_result = pthread_create(&thread_id, NULL, _background_task_wrapper, task_info);
    if (create_result != 0) return ERR_THREAD_CREATION_FAILED;
    *out_thread_id = thread_id;
    return SUCCESS;
}

#pragma endregion

#pragma region Private Registry Function Definitions

/**
 * @fn _register_background_task
 * @brief Registers a background task in the in-memory registry and assigns it a unique identifier.
 * This function creates a new entry in the background tasks registry for the provided task information and thread ID.
 * @param task_info Pointer to the background_task_t structure containing task information.
 * @param thread_id The pthread_t identifier of the created thread for the background task.
 * @param out_task_uuid Pointer to a uint32_t variable where the generated unique identifier for the task will be stored.
 * @return SUCCESS on success, or an error code on failure.
 */
int _register_background_task(background_task_t* task, pthread_t thread_id, uint32_t* out_task_uuid) {
    if(task == NULL || out_task_uuid == NULL) return ERR_INVALID_ARGUMENT;

    uint32_t task_uuid;
    int result = SUCCESS;

    result = _generate_unique_task_uuid(&task_uuid);
    if (result != 0) return result;

    background_task_info_t* task_info = allocate_memory(sizeof(background_task_info_t));
    if (!task_info) return ERR_MEMORY_ALLOCATION_FAILED;
    task_info->task_uuid = task_uuid;
    task_info->task = task;
    task_info->pthread_id = thread_id;

    result = _add_background_task_to_registry(task_info);
    if (result != 0) return result;

    *out_task_uuid = task_uuid;
    return result;
}

/**
 * @fn _add_background_task_to_registry
 * @brief Adds a background task to the in-memory registry.
 * This function adds the provided background task information to the registry, ensuring thread safety with a mutex lock.
 * @param task_info Pointer to the background_task_info_t structure containing task information to be added to the registry.
 * @return SUCCESS on success, or an error code on failure.
 */
int _add_background_task_to_registry(background_task_info_t* task_info) {
    
    int index = -1;
    int result = SUCCESS;

    pthread_mutex_lock(&registry_lock);
    
    if(active_tasks_count >= MAX_BACKGROUND_TASKS) {
        result = ERR_FAILURE; // Max capacity reached, cannot add more tasks
        index = -1;
    }
    else if(active_tasks_count == initialised_items_count){
        index = initialised_items_count; // Add to the next available slot
    }
    else {
        // Find an empty slot in the registry
        for (int i = 0; i < initialised_items_count; i++) {
            if (background_tasks_registry[i]== NULL) {
                index = i;
                break;
            }
        }
    }

    if(index == -1) {
        result = ERR_FAILURE; // This should not happen as we check for max capacity before
    }
    else {
        background_tasks_registry[index] = task_info;
        if(index == initialised_items_count) initialised_items_count++; // If we added to the next available slot, increment the count of initialized items
        active_tasks_count++;
    }
    
    pthread_mutex_unlock(&registry_lock);
    return result;
}

/**
 * @fn _get_background_task_info
 * @brief Retrieves information about a background task from the in-memory registry.
 * This function searches the registry for a background task with the specified unique identifier and returns its information.
 * @param task_uuid The unique identifier of the background task to retrieve.
 * @param out_task_info Pointer to a background_task_info_t pointer where the retrieved task information will be stored.
 * @return SUCCESS on success, or an error code on failure.
 */
int _get_background_task_info(uint32_t task_uuid, background_task_info_t** out_task_info) {
    if(out_task_info == NULL) return ERR_INVALID_ARGUMENT;
    if(task_uuid == UINT32_MAX) return ERR_INVALID_ARGUMENT;

    int result = SUCCESS;
    int index = -1;

    pthread_mutex_lock(&registry_lock);

    for (int i = 0; i < initialised_items_count; i++) {
        if(background_tasks_registry[i] == NULL) continue;
        if (background_tasks_registry[i]->task_uuid == task_uuid) {
            index = i;
            break;
        }
    }

    if(index != -1) {
        *out_task_info = background_tasks_registry[index];
    }
    else {
        result = ERR_FAILURE;
    }

    pthread_mutex_unlock(&registry_lock);
    return result;
}

/**
 * @fn _remove_background_task_from_registry
 * @brief Removes a background task from the in-memory registry.
 * This function removes the entry for the background task with the specified unique identifier from the registry and cleans up associated resources.
 * @param task_uuid The unique identifier of the background task to remove from the registry.
 * @return SUCCESS on success, or an error code on failure.
 */
int _remove_background_task_from_registry(uint32_t task_uuid) {
    if(task_uuid == UINT32_MAX) return ERR_INVALID_ARGUMENT;

    background_task_info_t* task_info;
    int index = -1;
    int result = SUCCESS;

    pthread_mutex_lock(&registry_lock);

    for (int i = 0; i < initialised_items_count; i++) {
        if(background_tasks_registry[i] == NULL) continue;
        if (background_tasks_registry[i]->task_uuid == task_uuid) {
            task_info = background_tasks_registry[i];
            index = i;
            break;
        }
    }

    if(task_info != NULL) {
        // Clean up resources associated with the background task
        free_memory(task_info->task->task_args, false);
        free_memory(task_info->task, false);
        free_memory(task_info, false);
        background_tasks_registry[index] = NULL; // Mark the slot as empty
        active_tasks_count--;
    }
    else {
        result = ERR_FAILURE;
    }

    pthread_mutex_unlock(&registry_lock);
    return result;
}

#pragma endregion