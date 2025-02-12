#include "internal/infrax/InfraxAsync.h"
#include "internal/infrax/InfraxCore.h"
#include "internal/infrax/InfraxMemory.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

// Forward declarations
static InfraxAsync* infrax_async_new(InfraxAsyncCallback callback, void* arg);
static void infrax_async_free(InfraxAsync* self);
static bool infrax_async_start(InfraxAsync* self);
static void infrax_async_cancel(InfraxAsync* self);
static bool infrax_async_is_done(InfraxAsync* self);
static int infrax_async_pollset_add_fd(InfraxAsync* self, int fd, short events, InfraxPollCallback callback, void* arg);
static void infrax_async_pollset_remove_fd(InfraxAsync* self, int fd);
static int infrax_async_pollset_poll(InfraxAsync* self, int timeout_ms);

// Thread-local pollset
__thread struct InfraxPollset* g_pollset = NULL;

// Global memory manager
static InfraxMemory* g_memory = NULL;
extern InfraxMemoryClassType InfraxMemoryClass;

// Global Core instance
static InfraxCore* g_core = NULL;

// Initialize pollset
static int pollset_init(struct InfraxPollset* ps, size_t initial_capacity) {
    if (!ps || initial_capacity == 0) return -1;
    
    ps->fds = (struct pollfd*)malloc(initial_capacity * sizeof(struct pollfd));
    ps->infos = (struct InfraxPollInfo**)malloc(initial_capacity * sizeof(struct InfraxPollInfo*));
    
    if (!ps->fds || !ps->infos) {
        free(ps->fds);
        free(ps->infos);
        return -1;
    }
    
    ps->size = 0;
    ps->capacity = initial_capacity;
    return 0;
}

// Clean up pollset
static void pollset_cleanup(struct InfraxPollset* ps) {
    if (!ps) return;
    
    for (size_t i = 0; i < ps->size; i++) {
        free(ps->infos[i]);
    }
    
    free(ps->fds);
    free(ps->infos);
    ps->fds = NULL;
    ps->infos = NULL;
    ps->size = 0;
    ps->capacity = 0;
}

// Ensure pollset is initialized
static int ensure_pollset() {
    if (g_pollset) return 0;
    
    g_pollset = (struct InfraxPollset*)malloc(sizeof(struct InfraxPollset));
    if (!g_pollset) return -1;
    
    if (pollset_init(g_pollset, 16) < 0) {
        free(g_pollset);
        g_pollset = NULL;
        return -1;
    }
    
    return 0;
}

// Add fd to pollset (class method)
static int infrax_async_pollset_add_fd(InfraxAsync* self, int fd, short events, InfraxPollCallback callback, void* arg) {
    if (fd < 0) return -1;
    if (ensure_pollset() < 0) return -1;
    
    // Check if fd already exists
    for (size_t i = 0; i < g_pollset->size; i++) {
        if (g_pollset->fds[i].fd == fd) {
            // Update existing entry
            g_pollset->fds[i].events = events;
            g_pollset->infos[i]->events = events;
            g_pollset->infos[i]->callback = callback;
            g_pollset->infos[i]->arg = arg;
            return 0;
        }
    }
    
    // Grow arrays if needed
    if (g_pollset->size >= g_pollset->capacity) {
        size_t new_capacity = g_pollset->capacity * 2;
        struct pollfd* new_fds = (struct pollfd*)realloc(g_pollset->fds, new_capacity * sizeof(struct pollfd));
        struct InfraxPollInfo** new_infos = (struct InfraxPollInfo**)realloc(g_pollset->infos, new_capacity * sizeof(struct InfraxPollInfo*));
        if (!new_fds || !new_infos) {
            free(new_fds);
            free(new_infos);
            return -1;
        }
        g_pollset->fds = new_fds;
        g_pollset->infos = new_infos;
        g_pollset->capacity = new_capacity;
    }
    
    // Add new entry
    struct InfraxPollInfo* info = (struct InfraxPollInfo*)malloc(sizeof(struct InfraxPollInfo));
    if (!info) return -1;
    
    info->fd = fd;
    info->events = events;
    info->callback = callback;
    info->arg = arg;
    info->next = NULL;
    
    g_pollset->fds[g_pollset->size].fd = fd;
    g_pollset->fds[g_pollset->size].events = events;
    g_pollset->fds[g_pollset->size].revents = 0;
    g_pollset->infos[g_pollset->size] = info;
    g_pollset->size++;
    
    return 0;
}

// Remove fd from pollset (class method)
static void infrax_async_pollset_remove_fd(InfraxAsync* self, int fd) {
    if (fd < 0 || !g_pollset) return;
    
    for (size_t i = 0; i < g_pollset->size; i++) {
        if (g_pollset->fds[i].fd == fd) {
            // Free the info structure
            free(g_pollset->infos[i]);
            
            // Move last element to this position if not last
            if (i < g_pollset->size - 1) {
                g_pollset->fds[i] = g_pollset->fds[g_pollset->size - 1];
                g_pollset->infos[i] = g_pollset->infos[g_pollset->size - 1];
            }
            
            g_pollset->size--;
            return;
        }
    }
}

// Poll events (class method)
static int infrax_async_pollset_poll(InfraxAsync* self, int timeout_ms) {
    if (!g_pollset || g_pollset->size == 0) return 0;
    
    // Use poll() to wait for events
    int ret = poll(g_pollset->fds, g_pollset->size, timeout_ms);
    if (ret < 0) return -1;
    
    // Process events
    for (size_t i = 0; i < g_pollset->size; i++) {
        if (g_pollset->fds[i].revents) {
            if (g_pollset->infos[i]->callback) {
                g_pollset->infos[i]->callback(self, g_pollset->fds[i].fd, 
                                            g_pollset->fds[i].revents, 
                                            g_pollset->infos[i]->arg);
            }
            g_pollset->fds[i].revents = 0;
        }
    }
    
    return ret;
}

// Initialize memory
static bool init_memory() {
    if (g_memory) return true;
    
    InfraxMemoryConfig config = {
        .initial_size = 1024 * 1024,  // 1MB
        .use_gc = false,
        .use_pool = true,
        .gc_threshold = 0
    };
    
    g_memory = InfraxMemoryClass.new(&config);
    g_core = InfraxCoreClass.singleton();
    
    if (!g_memory || !g_core) {
        if (g_memory) {
            InfraxMemoryClass.free(g_memory);
            g_memory = NULL;
        }
        return false;
    }
    
    return true;
}

// Create new InfraxAsync instance
static InfraxAsync* infrax_async_new(InfraxAsyncCallback callback, void* arg) {
    if (!init_memory()) return NULL;
    
    InfraxAsync* self = (InfraxAsync*)g_memory->alloc(g_memory, sizeof(InfraxAsync));
    if (!self) return NULL;
    
    memset(self, 0, sizeof(InfraxAsync));
    //self->self = self;//??
    self->klass = &InfraxAsyncClass;
    self->state = INFRAX_ASYNC_PENDING;
    self->callback = callback;
    self->arg = arg;
    
    return self;
}

// Free InfraxAsync instance
static void infrax_async_free(InfraxAsync* self) {
    if (!self) return;
    g_memory->dealloc(g_memory, self);
}

// Start async task
static bool infrax_async_start(InfraxAsync* self) {
    if (!self || !self->callback) return false;
    
    if (self->state == INFRAX_ASYNC_PENDING) {
        self->callback(self, self->arg);
        return true;
    }
    
    return false;
}

// Cancel async task
static void infrax_async_cancel(InfraxAsync* self) {
    if (!self) return;
    self->state = INFRAX_ASYNC_REJECTED;
}

// Check if task is done
static bool infrax_async_is_done(InfraxAsync* self) {
    if (!self) return true;
    return self->state == INFRAX_ASYNC_FULFILLED || self->state == INFRAX_ASYNC_REJECTED;
}

// Global class instance
InfraxAsyncClassType InfraxAsyncClass = {
    .new = infrax_async_new,
    .free = infrax_async_free,
    .start = infrax_async_start,
    .cancel = infrax_async_cancel,
    .is_done = infrax_async_is_done,
    .pollset_add_fd = infrax_async_pollset_add_fd,
    .pollset_remove_fd = infrax_async_pollset_remove_fd,
    .pollset_poll = infrax_async_pollset_poll
}; 