/**
 * @file InfraxThread.h
 * @brief Thread management functionality for the infrax subsystem
 */

#ifndef INFRAX_THREAD_H
#define INFRAX_THREAD_H

#include "InfraxCore.h"

typedef struct InfraxThread InfraxThread;

/**
 * @brief Create a new thread
 * @param name Thread name for identification
 * @param entry_point Function pointer to thread entry point
 * @param arg Argument to pass to thread function
 * @return Pointer to InfraxThread or NULL on failure
 */
InfraxThread* infrax_thread_create(const char* name, void* (*entry_point)(void*), void* arg);

/**
 * @brief Start the thread execution
 * @param thread Pointer to InfraxThread instance
 * @return 0 on success, error code on failure
 */
int infrax_thread_start(InfraxThread* thread);

/**
 * @brief Wait for thread completion
 * @param thread Pointer to InfraxThread instance
 * @param result Pointer to store thread result
 * @return 0 on success, error code on failure
 */
int infrax_thread_join(InfraxThread* thread, void** result);

/**
 * @brief Destroy thread resources
 * @param thread Pointer to InfraxThread instance
 */
void infrax_thread_destroy(InfraxThread* thread);

/**
 * @brief Get current thread ID
 * @return Thread ID of calling thread
 */
unsigned long infrax_thread_get_current_id(void);

#endif /* INFRAX_THREAD_H */
