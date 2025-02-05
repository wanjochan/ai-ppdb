/**
 * @file InfraxThread.c
 * @brief Implementation of thread management functionality
 */

#include "InfraxThread.h"
#include "InfraxError.h"
#include "InfraxMemory.h"
#include <pthread.h>

struct InfraxThread {
    pthread_t thread;
    char* name;
    void* (*entry_point)(void*);
    void* arg;
    int is_running;
};

InfraxThread* infrax_thread_create(const char* name, void* (*entry_point)(void*), void* arg) {
    if (!name || !entry_point) {
        return NULL;
    }

    InfraxThread* thread = infrax_malloc(sizeof(InfraxThread));
    if (!thread) {
        return NULL;
    }

    thread->name = infrax_strdup(name);
    if (!thread->name) {
        infrax_free(thread);
        return NULL;
    }

    thread->entry_point = entry_point;
    thread->arg = arg;
    thread->is_running = 0;

    return thread;
}

int infrax_thread_start(InfraxThread* thread) {
    if (!thread || thread->is_running) {
        return INFRAX_ERROR_INVALID_ARGUMENT;
    }

    int result = pthread_create(&thread->thread, NULL, thread->entry_point, thread->arg);
    if (result != 0) {
        return INFRAX_ERROR_THREAD_CREATE_FAILED;
    }

    thread->is_running = 1;
    return 0;
}

int infrax_thread_join(InfraxThread* thread, void** result) {
    if (!thread || !thread->is_running) {
        return INFRAX_ERROR_INVALID_ARGUMENT;
    }

    int ret = pthread_join(thread->thread, result);
    if (ret != 0) {
        return INFRAX_ERROR_THREAD_JOIN_FAILED;
    }

    thread->is_running = 0;
    return 0;
}

void infrax_thread_destroy(InfraxThread* thread) {
    if (!thread) {
        return;
    }

    if (thread->is_running) {
        infrax_thread_join(thread, NULL);
    }

    if (thread->name) {
        infrax_free(thread->name);
    }

    infrax_free(thread);
}

unsigned long infrax_thread_get_current_id(void) {
    return (unsigned long)pthread_self();
}
