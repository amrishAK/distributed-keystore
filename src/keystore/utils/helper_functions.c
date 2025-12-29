#include "helper_functions.h"
#include "type_definitions/error_code_definitions.h"
#include <stdlib.h>
#include <pthread.h>

// Windows-specific implementations can be added here if needed in the future

#pragma region Functions for Windows Platform Declarations
#ifdef _WIN32
#include <windows.h>
typedef struct {
    int (*background_task)(void*);
    void* task_args;
} background_task_args_t;

static DWORD WINAPI _background_task_wrapper(LPVOID lpParam);
#endif

int _initialize_background_function_windows(int (background_task)(void*), void* task_args);

#pragma endregion

#pragma region Functions for POSIX Platform Declarations
#ifdef __linux__
#include <pthread.h>
int _initialize_background_function_pthread(int (background_task)(void*), void* task_args);
#endif
#pragma endregion



#pragma region Public Function Definitions

int is_power_of_two(unsigned int bucket_size) {
    return (bucket_size != 0) && ((bucket_size & (bucket_size - 1)) == 0);
}

int initialize_background_function(int (background_task)(void*), void* task_args) {
    int result = 0;
    
    #ifdef _WIN32
        result = _initialize_background_function_windows(background_task, task_args);
    #elif __linux__
        result = _initialize_background_function_pthread(background_task, task_args);
    #else
        result = ERR_UNSUPPORTED_PLATFORM; // Error handling: unsupported platform
    #endif
    return result;
}

#pragma endregion


#pragma region Private Function Definitions

#ifdef __linux__
#include <pthread.h>
int _initialize_background_function_pthread(int (background_task)(void*), void* task_args) {
    pthread_t thread_id;
    background_task_args_t* args = malloc(sizeof(background_task_args_t));
    if (!args) return ERR_MEMORY_ALLOCATION_FAILED;
    args->background_task = background_task;
    args->task_args = task_args;
    int create_result = pthread_create(&thread_id, NULL, background_task_wrapper, args);
    if (create_result != 0) {
        free(args);
        return ERR_THREAD_CREATION_FAILED; // Error handling: thread creation failed
    }
    pthread_detach(thread_id); // Detach the thread to allow it to run independently
    return 0;
}
#endif
#pragma endregion


// Windows-specific implementations can be added here if needed in the future
#pragma region Functions for Windows Platform Definitions
#ifdef _WIN32
#include <windows.h>
static DWORD WINAPI _background_task_wrapper(LPVOID lpParam) {
    background_task_args_t* args = (background_task_args_t*)lpParam;
    if (args && args->background_task) {
        args->background_task(args->task_args);
        // Optionally: do something with result
    }
    free(args);
    return 0;
}

int _initialize_background_function_windows(int (*background_task)(void*), void* task_args) {
    background_task_args_t* args = (background_task_args_t*)malloc(sizeof(background_task_args_t));
    if (!args) return ERR_THREAD_CREATION_FAILED;
    args->background_task = background_task;
    args->task_args = task_args;
    HANDLE thread_handle = CreateThread(
        NULL,                   // default security attributes
        0,                      // use default stack size
        _background_task_wrapper, // thread function
        args,                   // argument to thread function
        0,                      // use default creation flags
        NULL);                  // returns the thread identifier
    if (thread_handle == NULL) {
        free(args);
        return ERR_THREAD_CREATION_FAILED;
    }
    CloseHandle(thread_handle); // Close the thread handle to allow it to run independently
    return 0;
}

#endif
#pragma endregion