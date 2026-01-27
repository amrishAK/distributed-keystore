/**
 * @file error_code_definitions.h
 * @brief Defines standardized error codes for the keystore project.
 * This header provides a set of predefined error codes to be used
 * across various modules of the distributed keystore for consistent error handling.
 * Each error code is represented as a negative integer, categorized by the type of error.
 * Categories include general errors, argument validation, memory/resource management,
 * concurrency/locking, hash/indexing, hash table operations, sub-hash table operations,
 * linked list operations, data node operations, and pool/manager specific errors.
 */
#ifndef ERROR_CODE_DEFINITIONS_H
#define ERROR_CODE_DEFINITIONS_H


// General Errors
#define ERR_FAILURE -1

// Argument/Validation Errors
#define ERR_INVALID_ARGUMENT -11
#define ERR_INVALID_CONFIG -12

// Memory/Resource Management (from 20 to 29)
#define ERR_MEMORY_ALLOCATION_FAILED -20
#define ERR_RESOURCE_INIT_FAILED -21
#define ERR_RESOURCE_CLEANUP_FAILED -22

// Concurrency/Locking (from 30 to 39)
#define ERR_RW_LOCK_ACQUIRE_FAILED -30
#define ERR_RW_LOCK_RELEASE_FAILED -31
#define ERR_MUTEX_LOCK_ACQUIRE_FAILED -32
#define ERR_MUTEX_LOCK_RELEASE_FAILED -33
#define ERR_GENERIC_LOCK_ACQUIRE_FAILED -34
#define ERR_GENERIC_LOCK_RELEASE_FAILED -35

// Hash/Indexing Errors (from 40 to 49)
#define ERR_HASH_COMPUTE_FAILED -40
#define ERR_INVALID_BUCKET_INDEX -41
#define ERR_INVALID_SUB_BUCKET_INDEX -42

// Hash Table/Bucket Operation Errors (from 50 to 59)
#define ERR_HASH_TABLE_NOT_INITIALIZED -50
#define ERR_HASH_BUCKET_NOT_FOUND -51
#define ERR_UNSUPPORTED_HASH_BUCKET_OP -52
#define ERR_HASH_BUCKET_FULL -53
#define ERR_HASH_BUCKET_NOT_INITIALIZED -54

// Sub Hash Table Operation Errors (from 60 to 69)
#define ERR_SUB_HASH_TABLE_NOT_INITIALIZED -60
#define ERR_SUB_HASH_BUCKET_NOT_FOUND -61
#define ERR_UNSUPPORTED_SUB_HASH_TABLE_OP -62
#define ERR_SUB_HASH_TABLE_RESIZE_FAILED -63
#define ERR_SUB_HASH_BUCKET_NOT_INITIALIZED -64

// Linked List Operation Errors (from 70 to 79)
#define ERR_LINKED_LIST_NODE_NOT_FOUND -70
#define ERR_LINKED_LIST_NODE_CREATION_FAILED -71
#define ERR_UNSUPPORTED_LINKED_LIST_OP -72

// Data Node Operation Errors (from 80 to 89)
#define ERR_DATA_NODE_NOT_FOUND -80
#define ERR_DATA_NODE_UPDATE_FAILED -81
#define ERR_UNSUPPORTED_DATA_NODE_OP -82
#define ERR_DATA_NODE_CREATION_FAILED -83

// Pool/Manager Specific (from 90 to 99)
#define ERR_POOL_NOT_INITIALIZED -90
#define ERR_POOL_EXHAUSTED -91
#define ERR_POOL_CORRUPTION_DETECTED -92

// Additional error codes can be defined here as needed.
#define ERR_UNSUPPORTED_PLATFORM -100
#define ERR_THREAD_CREATION_FAILED -101
#define ERR_MAX_RETRY_EXCEEDED -102

#endif // ERROR_CODE_DEFINITIONS_H