#ifndef BACKGROUND_TASK_MANAGER_TYPE_DEFINITIONS_H
#define BACKGROUND_TASK_MANAGER_TYPE_DEFINITIONS_H


#include <pthread.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    void* task_input_args;
    _Atomic bool kill_signal; // Flag to signal the background task to terminate gracefully
} background_task_args_t;

typedef struct {
    int (*background_task)(void*);
    background_task_args_t* task_args;
    bool is_detached;
} background_task_t;

typedef struct 
{
    uint32_t task_uuid;
    background_task_t* task;
    pthread_t pthread_id;
} background_task_info_t;


#endif // BACKGROUND_TASK_MANAGER_TYPE_DEFINITIONS_H