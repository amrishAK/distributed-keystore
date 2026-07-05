# Background Task Manager

Manages thread creation, lifecycle, and cleanup for background workers (resize and chase workers). Supports both detached (fire-and-forget) and joinable (managed) task models.

## Table of Contents
- [1. Overview](#1-overview)
- [2. Task State Machine](#2-task-state-machine)
- [3. Task Models](#3-task-models)
- [4. Core Operations](#4-core-operations)
- [5. Thread Deployment](#5-thread-deployment)
- [6. Implementation Concerns](#6-implementation-concerns)

## 1. Overview

```text
┌────────────────────────────────────────────────────┐
│         Background Task Manager                    │
│                                                    │
│  • Registry: background_tasks_registry[100]        │
│  • Lock: pthread_mutex_t registry_lock             │
│  • Supports: Detached and joinable tasks           │
│  • Detached tasks: self-managed, no UUID           │
│  • Joinable tasks: registered, managed via UUID    │
└────────────────────────────────────────────────────┘
```

## 2. Task State Machine

```text
       ┌──────────────┐
       │  Unallocated │
       └──────┬───────┘
              │
              │ initialize_background_function()
              ▼
       ┌──────────────┐
       │  Allocated   │  ◄─── pthread_create() + pthread_detach()
       │  + Started   │         (detached path)
       └──────┬───────┘
              │
              ├─────────────────────────────────────┐
              │                                     │
              │ (detached)                          │ (joinable)
              ▼                                     ▼
       ┌──────────────┐                   ┌──────────────┐
       │   Running    │                   │  Registered  │
       │  (Unmanaged) │                   │  + Running   │
       └──────┬───────┘                   └──────┬───────┘
              │                                  │
              │ thread exits                     │ cleanup_background_task(uuid)
              ▼                                  ▼
       ┌──────────────┐                   ┌──────────────┐
       │   Exited     │                   │  Signaled    │
       │  + Cleaned   │                   │  (kill_flag) │
       └──────────────┘                   └──────┬───────┘
                                                 │
                                                 │ pthread_join()
                                                 ▼
                                         ┌──────────────┐
                                         │  Joined      │
                                         │  + Unregistered
                                         │  + Cleaned   │
                                         └──────────────┘
```

## 3. Task Models

| Model | Detached | Management | Registry | UUID | Cleanup |
|-------|----------|-----------|----------|------|---------|
| **Detached** | Yes | Self-managed | No | `0` | Automatic (thread exit) |
| **Joinable** | No | Externally managed | Yes | Assigned | Explicit `cleanup_background_task(uuid)` |

## 4. Core Operations

### `initialize_background_function()`

Creates and starts a background task.

**Steps:**
1. Allocate `background_task_args_t` with atomic `kill_signal = false`
2. Allocate `background_task_t` with function pointer and detached flag
3. Call `pthread_create()` to spawn thread
4. **If detached:** Call `pthread_detach()`, return `task_uuid_out = 0`
5. **If joinable:** Register in registry with UUID, return UUID

**Memory responsibility:**
- Detached: thread frees its own wrapper structs on exit
- Joinable: caller must call `cleanup_background_task(uuid)` to free

### `cleanup_background_task(uuid)`

Gracefully signals and joins a registered task.

**Steps:**
1. Look up task in registry by UUID
2. Set `kill_signal = true` (atomic, non-blocking)
3. Call `pthread_join()` (block until thread exits)
4. Unregister from registry
5. Free task structs and args

> **Note:** Only callable on joinable tasks. Detached tasks (`uuid = 0`) exit autonomously.

## 5. Thread Deployment

| Worker | Model | Role |
|--------|-------|------|
| **Resize worker** | Detached | Migrate snapshot data into new sub-hash-table; autonomous cleanup |
| **Chase worker** | Joinable | Drain `new_operation_buffer`; managed cleanup during finalization |

## 6. Implementation Concerns

### Concurrency & Safety

- Registry access protected by `registry_lock` (pthread_mutex)
- `kill_signal` is `_Atomic bool` for lock-free signaling
- Worker threads must poll `kill_signal` to exit gracefully

### Edge Cases

| Case | Behavior |
|------|----------|
| Double cleanup | Caller error; registry lookup fails, return error code |
| Thread crashes before join | `pthread_join()` still succeeds, return code indicates failure |
| Registry full (100 tasks) | Fail to register; return error (max concurrent joinable tasks = 100) |
| Cleanup called during task startup race | Lock-based registry prevents inconsistency |

### Error Handling

- `initialize_background_function()` returns error code if `pthread_create()` fails
- Registry full returns distinct error code
- `cleanup_background_task()` returns error if UUID not found

> **Warning:** Do not attempt to cleanup the same UUID twice or cleanup a detached task (UUID = 0). Behavior is undefined.
