/**
 * @file InfraxThread.c
 * @brief Implementation of thread management functionality
 */

#include "InfraxThread.h"
#include "InfraxMemory.h"
#include "InfraxCore.h"
#include <pthread.h>
#include <stdbool.h>
#include <string.h>

// Instance methods implementation
static InfraxError thread_start(InfraxThread* self) {
    InfraxCore* core = InfraxCoreClass.singleton();
    
    if (!self || self->is_running) {
        return make_error(INFRAX_ERROR_INVALID_ARGUMENT, "Invalid thread or thread already running");
    }

    int result = pthread_create(&self->native_handle, NULL, 
                              self->config.entry_point, 
                              self->config.arg);
    if (result != 0) {
        return make_error(INFRAX_ERROR_THREAD_CREATE_FAILED, "Failed to create thread");
    }

    self->is_running = true;
    return make_error(0, "");
}

static InfraxError thread_join(InfraxThread* self, void** result) {
    InfraxCore* core = InfraxCoreClass.singleton();
    
    if (!self || !self->is_running) {
        return make_error(INFRAX_ERROR_INVALID_ARGUMENT, "Invalid thread or thread not running");
    }

    int ret = pthread_join(self->native_handle, result);
    if (ret != 0) {
        return make_error(INFRAX_ERROR_THREAD_JOIN_FAILED, "Failed to join thread");
    }

    self->is_running = false;
    if (result) {
        self->result = *result;
    }
    return make_error(0, "");
}

static InfraxThreadId thread_tid(InfraxThread* self) {
    if (!self || !self->is_running) {
        return 0;
    }
    return (InfraxThreadId)self->native_handle;
}

// Static methods implementation
static InfraxThread* infrax_thread_new(const InfraxThreadConfig* config) {
    if (!config || !config->name || !config->entry_point) {
        return NULL;
    }

    // 获取全局内存管理器
    InfraxMemoryConfig mem_config = {
        .initial_size = 64 * 1024,
        .use_gc = true,
        .use_pool = true,
        .gc_threshold = 32 * 1024
    };
    InfraxMemory* memory = InfraxMemory_CLASS.new(&mem_config);
    if (!memory) {
        return NULL;
    }

    InfraxThread* self = memory->alloc(memory, sizeof(InfraxThread));
    if (!self) {
        InfraxMemory_CLASS.free(memory);
        return NULL;
    }

    // 复制配置
    self->klass = &InfraxThread_CLASS;
    self->config.name = memory->alloc(memory, strlen(config->name) + 1);
    if (!self->config.name) {
        memory->dealloc(memory, self);
        InfraxMemory_CLASS.free(memory);
        return NULL;
    }
    strcpy((char*)self->config.name, config->name);
    self->config.entry_point = config->entry_point;
    self->config.arg = config->arg;
    self->is_running = false;
    self->result = NULL;

    // Initialize instance methods
    self->start = thread_start;
    self->join = thread_join;
    self->tid = thread_tid;

    return self;
}

static void infrax_thread_free(InfraxThread* self) {
    if (!self) {
        return;
    }

    if (self->is_running) {
        self->join(self, NULL);
    }

    // 获取全局内存管理器
    InfraxMemoryConfig mem_config = {
        .initial_size = 64 * 1024,
        .use_gc = true,
        .use_pool = true,
        .gc_threshold = 32 * 1024
    };
    InfraxMemory* memory = InfraxMemory_CLASS.new(&mem_config);
    if (!memory) {
        return;
    }

    if (self->config.name) {
        memory->dealloc(memory, (void*)self->config.name);
    }
    memory->dealloc(memory, self);
    InfraxMemory_CLASS.free(memory);
}

// The class instance
const InfraxThreadClass InfraxThread_CLASS = {
    .new = infrax_thread_new,
    .free = infrax_thread_free
};
