/**
 * @file InfraxThread.c
 * @brief Implementation of thread management functionality
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "internal/infrax/InfraxThread.h"
#include "internal/infrax/InfraxMemory.h"

// Forward declarations of instance methods
static InfraxError thread_start(InfraxThread* self);
static InfraxError thread_join(InfraxThread* self, void** result);
static InfraxThreadId thread_tid(InfraxThread* self);

// Thread function wrapper
static void* thread_func(void* arg) {
    InfraxThread* self = (InfraxThread*)arg;
    if (!self || !self->config.entry_point) return NULL;
    
    self->result = self->config.entry_point(self->config.arg);
    return self->result;
}

// Get or create memory manager
static InfraxMemory* get_memory_manager(void) {
    static InfraxMemory* memory = NULL;
    if (!memory) {
        InfraxMemoryConfig config = {
            .initial_size = 1024 * 1024,  // 1MB
            .use_gc = false,
            .use_pool = true,
            .gc_threshold = 0
        };
        memory = InfraxMemoryClass.new(&config);
    }
    return memory;
}

// Constructor implementation
static InfraxThread* thread_new(const InfraxThreadConfig* config) {
    if (!config || !config->name || !config->entry_point) {
        return NULL;
    }

    // Get memory manager
    InfraxMemory* memory = get_memory_manager();
    if (!memory) {
        return NULL;
    }

    // Allocate thread object
    InfraxThread* self = (InfraxThread*)memory->alloc(memory, sizeof(InfraxThread));
    if (!self) {
        return NULL;
    }

    // Initialize thread object
    memset(self, 0, sizeof(InfraxThread));
    
    // Copy configuration
    size_t name_len = strlen(config->name) + 1;
    char* name_copy = (char*)memory->alloc(memory, name_len);
    if (!name_copy) {
        memory->dealloc(memory, self);
        return NULL;
    }
    memcpy(name_copy, config->name, name_len);
    self->config.name = name_copy;
    self->config.entry_point = config->entry_point;
    self->config.arg = config->arg;
    
    // Set instance methods
    self->start = thread_start;
    self->join = thread_join;
    self->tid = thread_tid;
    
    return self;
}

// Destructor implementation
static void thread_free(InfraxThread* self) {
    if (!self) return;
    
    InfraxMemory* memory = get_memory_manager();
    if (!memory) return;
    
    if (self->is_running) {
        pthread_cancel(self->native_handle);
        pthread_join(self->native_handle, NULL);
    }
    
    if (self->config.name) {
        memory->dealloc(memory, (void*)self->config.name);
    }
    memory->dealloc(memory, self);
}

// The "static" interface implementation
const InfraxThreadClassType InfraxThreadClass = {
    .new = thread_new,
    .free = thread_free
};

// Instance methods implementation
static InfraxError thread_start(InfraxThread* self) {
    if (!self || !self->config.entry_point) {
        return make_error(INFRAX_ERROR_INVALID_ARGUMENT, "Invalid thread or entry point");
    }

    if (self->is_running) {
        return make_error(INFRAX_ERROR_INVALID_ARGUMENT, "Thread already running");
    }

    pthread_attr_t attr;
    if (pthread_attr_init(&attr) != 0) {
        return make_error(INFRAX_ERROR_THREAD_CREATE_FAILED, "Failed to initialize thread attributes");
    }

    if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE) != 0) {
        pthread_attr_destroy(&attr);
        return make_error(INFRAX_ERROR_THREAD_CREATE_FAILED, "Failed to set thread detach state");
    }

    int rc = pthread_create(&self->native_handle, &attr, thread_func, self);
    pthread_attr_destroy(&attr);

    if (rc != 0) {
        return make_error(INFRAX_ERROR_THREAD_CREATE_FAILED, "Failed to create thread");
    }

    // Set running state after successful thread creation
    self->is_running = true;

    return make_error(0, "Success");
}

static InfraxError thread_join(InfraxThread* self, void** result) {
    if (!self) {
        return make_error(INFRAX_ERROR_INVALID_ARGUMENT, "Invalid thread");
    }

    if (!self->is_running) {
        return make_error(INFRAX_ERROR_THREAD_JOIN_FAILED, "Thread not running");
    }

    int rc = pthread_join(self->native_handle, result);
    if (rc != 0) {
        return make_error(INFRAX_ERROR_THREAD_JOIN_FAILED, "Failed to join thread");
    }

    // Set running state after successful join
    self->is_running = false;

    return make_error(0, "Success");
}

static InfraxThreadId thread_tid(InfraxThread* self) {
    if (!self) {
        return 0;
    }
    return (InfraxThreadId)self->native_handle;
}
