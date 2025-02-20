#include "internal/infrax/InfraxAsync.h"
#include "internal/infrax/InfraxCore.h"
#include "internal/infrax/InfraxMemory.h"

// Poll events
#define INFRAX_POLLIN  0x001
#define INFRAX_POLLOUT 0x004
#define INFRAX_POLLERR 0x008
#define INFRAX_POLLHUP 0x010

// Timer constants
#define INITIAL_TIMER_CAPACITY 1024  // Initial capacity for timer arrays
#define INVALID_TIMER_ID 0

// Timer structure
typedef struct {
    InfraxU32 id;
    InfraxU64 expire_time;
    InfraxU32 interval_ms;
    InfraxBool is_interval;
    InfraxBool is_valid;
    InfraxPollCallback handler;
    void* arg;
} InfraxTimer;

// Timer system
typedef struct {
    InfraxTimer* timers;     // Dynamic timer pool
    InfraxTimer** heap;      // Dynamic min heap
    InfraxSize capacity;     // Current capacity
    InfraxSize heap_size;    // Current heap size
    InfraxU32 next_id;
    InfraxBool initialized;
} InfraxTimerSystem;

static InfraxTimerSystem g_timers = {0};

// Poll info structure
struct InfraxPollInfo {
    int fd;
    short events;
    InfraxPollCallback callback;
    void* arg;
    struct InfraxPollInfo* next;
};

// Poll structure
struct InfraxPollset {
    InfraxPollFd* fds;
    struct InfraxPollInfo** infos;
    InfraxSize size;
    InfraxSize capacity;
};

// 堆操作辅助函数
static void heap_swap(InfraxSize i, InfraxSize j) {
    InfraxTimer* temp = g_timers.heap[i];
    g_timers.heap[i] = g_timers.heap[j];
    g_timers.heap[j] = temp;
}

static void heap_up(InfraxSize pos) {
    while (pos > 0) {
        InfraxSize parent = (pos - 1) / 2;
        if (g_timers.heap[parent]->expire_time <= g_timers.heap[pos]->expire_time) {
            break;
        }
        heap_swap(parent, pos);
        pos = parent;
    }
}

static void heap_down(InfraxSize pos) {
    while (INFRAX_TRUE) {
        InfraxSize min_pos = pos;
        InfraxSize left = 2 * pos + 1;
        InfraxSize right = 2 * pos + 2;
        
        if (left < g_timers.heap_size && 
            g_timers.heap[left]->expire_time < g_timers.heap[min_pos]->expire_time) {
            min_pos = left;
        }
        
        if (right < g_timers.heap_size && 
            g_timers.heap[right]->expire_time < g_timers.heap[min_pos]->expire_time) {
            min_pos = right;
        }
        
        if (min_pos == pos) break;
        
        heap_swap(pos, min_pos);
        pos = min_pos;
    }
}

static void remove_timer_from_heap(InfraxTimer* timer) {
    InfraxSize pos;
    for (pos = 0; pos < g_timers.heap_size; pos++) {
        if (g_timers.heap[pos] == timer) break;
    }
    
    if (pos == g_timers.heap_size) return;  // Not found
    
    // Replace with last element and remove last
    g_timers.heap[pos] = g_timers.heap[--g_timers.heap_size];
    
    // Restore heap property
    if (pos > 0 && g_timers.heap[pos]->expire_time < g_timers.heap[(pos-1)/2]->expire_time) {
        heap_up(pos);
    } else {
        heap_down(pos);
    }
}

// Thread-local pollset
__thread struct InfraxPollset* g_pollset = NULL;

// Global memory manager
static InfraxMemory* g_memory = NULL;
extern InfraxMemoryClassType InfraxMemoryClass;

// Global Core instance
static InfraxCore* g_core = NULL;

// Initialize timer system
static InfraxBool init_timer_system(void) {
    if (g_timers.initialized) return INFRAX_TRUE;
    
    // Allocate initial arrays
    g_timers.timers = (InfraxTimer*)g_memory->alloc(g_memory, INITIAL_TIMER_CAPACITY * sizeof(InfraxTimer));
    g_timers.heap = (InfraxTimer**)g_memory->alloc(g_memory, INITIAL_TIMER_CAPACITY * sizeof(InfraxTimer*));
    
    if (!g_timers.timers || !g_timers.heap) {
        if (g_timers.timers) g_memory->dealloc(g_memory, g_timers.timers);
        if (g_timers.heap) g_memory->dealloc(g_memory, g_timers.heap);
        return INFRAX_FALSE;
    }
    
    g_timers.capacity = INITIAL_TIMER_CAPACITY;
    g_timers.heap_size = 0;
    g_timers.next_id = 1;  // Start from 1
    g_timers.initialized = INFRAX_TRUE;
    
    return INFRAX_TRUE;
}

// Expand timer arrays
static InfraxBool expand_timer_arrays(void) {
    InfraxSize new_capacity = g_timers.capacity * 2;
    
    // Allocate new arrays
    InfraxTimer* new_timers = (InfraxTimer*)g_memory->alloc(g_memory, new_capacity * sizeof(InfraxTimer));
    InfraxTimer** new_heap = (InfraxTimer**)g_memory->alloc(g_memory, new_capacity * sizeof(InfraxTimer*));
    
    if (!new_timers || !new_heap) {
        if (new_timers) g_memory->dealloc(g_memory, new_timers);
        if (new_heap) g_memory->dealloc(g_memory, new_heap);
        return INFRAX_FALSE;
    }
    
    // Copy existing data
    g_core->memcpy(g_core, new_timers, g_timers.timers, g_timers.capacity * sizeof(InfraxTimer));
    g_core->memcpy(g_core, new_heap, g_timers.heap, g_timers.heap_size * sizeof(InfraxTimer*));
    
    // Update heap pointers
    for (InfraxSize i = 0; i < g_timers.heap_size; i++) {
        InfraxSize offset = g_timers.heap[i] - g_timers.timers;
        new_heap[i] = new_timers + offset;
    }
    
    // Free old arrays
    g_memory->dealloc(g_memory, g_timers.timers);
    g_memory->dealloc(g_memory, g_timers.heap);
    
    // Update system state
    g_timers.timers = new_timers;
    g_timers.heap = new_heap;
    g_timers.capacity = new_capacity;
    
    return INFRAX_TRUE;
}

// Add timer to heap
static InfraxBool add_timer_to_heap(InfraxTimer* timer) {
    if (!timer || !timer->is_valid) return INFRAX_FALSE;
    
    // Check if expansion is needed
    if (g_timers.heap_size >= g_timers.capacity) {
        if (!expand_timer_arrays()) return INFRAX_FALSE;
    }
    
    InfraxSize pos = g_timers.heap_size++;
    g_timers.heap[pos] = timer;
    heap_up(pos);
    
    return INFRAX_TRUE;
}

// Create new timer
static InfraxU32 create_timer(InfraxU32 interval_ms, InfraxPollCallback handler, void* arg, InfraxBool is_interval) {
    if (!init_timer_system() || !handler) return INVALID_TIMER_ID;
    
    // Find free timer slot
    InfraxTimer* timer = NULL;
    for (InfraxSize i = 0; i < g_timers.capacity; i++) {
        if (!g_timers.timers[i].is_valid) {
            timer = &g_timers.timers[i];
            break;
        }
    }
    
    // If no free slot, try to expand
    if (!timer) {
        if (!expand_timer_arrays()) return INVALID_TIMER_ID;
        timer = &g_timers.timers[g_timers.capacity / 2];  // Use first slot in new space
    }
    
    // Initialize timer
    timer->id = g_timers.next_id++;
    timer->expire_time = g_core->time_monotonic_ms(g_core) + interval_ms;
    timer->interval_ms = interval_ms;
    timer->is_interval = is_interval;
    timer->is_valid = INFRAX_TRUE;
    timer->handler = handler;
    timer->arg = arg;
    
    // Add to heap
    if (!add_timer_to_heap(timer)) {
        timer->is_valid = INFRAX_FALSE;
        return INVALID_TIMER_ID;
    }
    
    return timer->id;
}

// Set timeout
static InfraxU32 infrax_async_set_timeout(InfraxU32 interval_ms, InfraxPollCallback handler, void* arg) {
    return create_timer(interval_ms, handler, arg, INFRAX_FALSE);
}

// Set interval
static InfraxU32 infrax_async_set_interval(InfraxU32 interval_ms, InfraxPollCallback handler, void* arg) {
    return create_timer(interval_ms, handler, arg, INFRAX_TRUE);
}

// Clear timer
static InfraxError infrax_async_clear_timer(InfraxU32 timer_id) {
    InfraxError err = {0};
    
    if (timer_id == INVALID_TIMER_ID) return err;
    
    // Find timer
    InfraxTimer* timer = NULL;
    for (int i = 0; i < g_timers.capacity; i++) {
        if (g_timers.timers[i].is_valid && g_timers.timers[i].id == timer_id) {
            timer = &g_timers.timers[i];
            break;
        }
    }
    
    if (timer) {
        remove_timer_from_heap(timer);
        timer->is_valid = false;
    }
    
    return err;
}

// Initialize memory
static bool init_memory(void) {
    if (g_memory) return true;
    
    InfraxMemoryConfig config = {
        .initial_size = 1024 * 1024,  // 1MB
        .use_gc = false,
        .use_pool = true,
        .gc_threshold = 0
    };
    
    g_memory = InfraxMemoryClass.new(&config);
    g_core = InfraxCoreClass.singleton();
    
    return (g_memory != NULL && g_core != NULL);
}

// Initialize pollset
static int pollset_init(struct InfraxPollset* ps, InfraxSize initial_capacity) {
    if (!ps || initial_capacity == 0 || !g_memory) return -1;
    
    ps->fds = (InfraxPollFd*)g_memory->alloc(g_memory, initial_capacity * sizeof(InfraxPollFd));
    ps->infos = (struct InfraxPollInfo**)g_memory->alloc(g_memory, initial_capacity * sizeof(struct InfraxPollInfo*));
    
    if (!ps->fds || !ps->infos) {
        if (ps->fds) g_memory->dealloc(g_memory, ps->fds);
        if (ps->infos) g_memory->dealloc(g_memory, ps->infos);
        return -1;
    }
    
    ps->size = 0;
    ps->capacity = initial_capacity;
    return 0;
}

// Clean up pollset
static void pollset_cleanup(struct InfraxPollset* ps) {
    if (!ps || !g_memory) return;
    
    for (InfraxSize i = 0; i < ps->size; i++) {
        if (ps->infos[i]) g_memory->dealloc(g_memory, ps->infos[i]);
    }
    
    if (ps->fds) g_memory->dealloc(g_memory, ps->fds);
    if (ps->infos) g_memory->dealloc(g_memory, ps->infos);
    ps->fds = NULL;
    ps->infos = NULL;
    ps->size = 0;
    ps->capacity = 0;
}

// Ensure pollset is initialized
static int ensure_pollset(void) {
    if (g_pollset) return 0;
    
    g_pollset = (struct InfraxPollset*)g_memory->alloc(g_memory, sizeof(struct InfraxPollset));
    if (!g_pollset) return -1;
    
    if (pollset_init(g_pollset, 16) < 0) {
        g_memory->dealloc(g_memory, g_pollset);
        g_pollset = NULL;
        return -1;
    }
    
    return 0;
}

// Add file descriptor to pollset
static int infrax_async_pollset_add_fd(InfraxAsync* self, int fd, short events, InfraxPollCallback callback, void* arg) {
    if (!g_pollset || fd < 0) return -1;
    
    // Check if fd already exists
    for (InfraxSize i = 0; i < g_pollset->size; i++) {
        if (g_pollset->fds[i].fd == fd) {
            // Update existing entry
            g_pollset->fds[i].events = events;
            g_pollset->infos[i]->callback = callback;
            g_pollset->infos[i]->arg = arg;
            return 0;
        }
    }
    
    // Check capacity
    if (g_pollset->size >= g_pollset->capacity) {
        InfraxSize new_capacity = g_pollset->capacity * 2;
        InfraxPollFd* new_fds = (InfraxPollFd*)g_memory->alloc(g_memory, new_capacity * sizeof(InfraxPollFd));
        struct InfraxPollInfo** new_infos = (struct InfraxPollInfo**)g_memory->alloc(g_memory, new_capacity * sizeof(struct InfraxPollInfo*));
        
        if (!new_fds || !new_infos) {
            if (new_fds) g_memory->dealloc(g_memory, new_fds);
            if (new_infos) g_memory->dealloc(g_memory, new_infos);
            return -1;
        }
        
        g_core->memcpy(g_core, new_fds, g_pollset->fds, g_pollset->size * sizeof(InfraxPollFd));
        g_core->memcpy(g_core, new_infos, g_pollset->infos, g_pollset->size * sizeof(struct InfraxPollInfo*));
        
        g_memory->dealloc(g_memory, g_pollset->fds);
        g_memory->dealloc(g_memory, g_pollset->infos);
        
        g_pollset->fds = new_fds;
        g_pollset->infos = new_infos;
        g_pollset->capacity = new_capacity;
    }
    
    // Add new entry
    struct InfraxPollInfo* info = (struct InfraxPollInfo*)g_memory->alloc(g_memory, sizeof(struct InfraxPollInfo));
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

// Remove file descriptor from pollset
static void infrax_async_pollset_remove_fd(InfraxAsync* self, int fd) {
    if (!g_pollset || fd < 0) return;
    
    for (size_t i = 0; i < g_pollset->size; i++) {
        if (g_pollset->fds[i].fd == fd) {
            // Free info
            if (g_pollset->infos[i]) {
                g_memory->dealloc(g_memory, g_pollset->infos[i]);
            }
            
            // Move last entry to this position
            if (i < g_pollset->size - 1) {
                g_pollset->fds[i] = g_pollset->fds[g_pollset->size - 1];
                g_pollset->infos[i] = g_pollset->infos[g_pollset->size - 1];
            }
            
            g_pollset->size--;
            break;
        }
    }
}

// Poll for events
static int infrax_async_pollset_poll(InfraxAsync* self, int timeout_ms) {
    if (!g_pollset) return 0;
    
    // Check timers
    InfraxU64 now = g_core->time_monotonic_ms(g_core);
    InfraxBool timer_triggered = INFRAX_FALSE;
    
    while (g_timers.heap_size > 0 && g_timers.heap[0]->expire_time <= now) {
        InfraxTimer* timer = g_timers.heap[0];
        if (timer->is_valid) {
            // Call handler
            if (timer->handler) {
                timer->handler(self, -1, INFRAX_POLLIN, timer->arg);
                timer_triggered = INFRAX_TRUE;
            }
            
            if (timer->is_interval) {
                // Update timer for next interval
                timer->expire_time = now + timer->interval_ms;
                remove_timer_from_heap(timer);
                add_timer_to_heap(timer);
            } else {
                // Remove one-shot timer
                timer->is_valid = INFRAX_FALSE;
                remove_timer_from_heap(timer);
            }
        } else {
            // Remove invalid timer
            remove_timer_from_heap(timer);
        }
    }
    
    // If timer triggered, poll immediately
    if (timer_triggered) {
        timeout_ms = 0;
    }
    
    // Poll with timeout
    int ret = g_core->poll(g_core, g_pollset->fds, g_pollset->size, timeout_ms);
    if (ret < 0) {
        int err = g_core->get_last_error(g_core);
        if (err == INFRAX_EINTR) return 0;
        return -1;
    }
    
    // Process events in batch
    if (ret > 0) {
        // Process all ready events
        for (InfraxSize i = 0; i < g_pollset->size; i++) {
            if (g_pollset->fds[i].revents) {
                struct InfraxPollInfo* info = g_pollset->infos[i];
                if (info && info->callback) {
                    info->callback(self, info->fd, g_pollset->fds[i].revents, info->arg);
                }
                g_pollset->fds[i].revents = 0;
            }
        }
    }
    
    return ret;
}

// Create new InfraxAsync instance
static InfraxAsync* infrax_async_new(InfraxAsyncCallback callback, void* arg) {
    if (!init_memory()) return NULL;
    
    InfraxAsync* self = (InfraxAsync*)g_memory->alloc(g_memory, sizeof(InfraxAsync));
    if (!self) return NULL;
    
    gInfraxCore.memset(&gInfraxCore, self, 0, sizeof(InfraxAsync));
    self->klass = &InfraxAsyncClass;
    self->state = INFRAX_ASYNC_PENDING;
    self->callback = callback;
    self->arg = arg;
    
    // Initialize pollset
    if (ensure_pollset() < 0) {
        g_memory->dealloc(g_memory, self);
        return NULL;
    }
    
    return self;
}

// Free InfraxAsync instance
static void infrax_async_free(InfraxAsync* self) {
    if (!self) return;
    
    // Clean up pollset
    if (g_pollset) {
        pollset_cleanup(g_pollset);
        g_memory->dealloc(g_memory, g_pollset);
        g_pollset = NULL;
    }
    
    g_memory->dealloc(g_memory, self);
}

// Start async task
static bool infrax_async_start(InfraxAsync* self) {
    if (!self || !self->callback) return false;
    if (self->state != INFRAX_ASYNC_PENDING) return false;
    
    self->state = INFRAX_ASYNC_TMP;
    self->callback(self, self->arg);
    
    if (self->state == INFRAX_ASYNC_TMP) {
        self->state = INFRAX_ASYNC_FULFILLED;
    }
    
    return true;
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
    .pollset_poll = infrax_async_pollset_poll,
    .setTimeout = infrax_async_set_timeout,
    .clearTimeout = infrax_async_clear_timer,
    .setInterval = infrax_async_set_interval,
    .clearInterval = infrax_async_clear_timer
};