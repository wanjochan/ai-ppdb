#include "internal/infrax/InfraxTimer.h"
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <pthread.h>

#define WHEEL_SIZE 256  // Must be power of 2
#define WHEEL_MASK (WHEEL_SIZE - 1)
#define WHEEL_MS 1000   // Time wheel resolution in milliseconds

// Forward declarations
static void infrax_timer_stop(InfraxTimer* timer);
static InfraxError ensure_timer_thread(void);

// Timer implementation
struct InfraxTimer {
    int timeout_ms;              // Timer timeout in milliseconds
    InfraxTimerCallback callback;// Callback function
    void* callback_arg;          // Callback argument
    _Atomic int active;          // Timer is active (atomic)
    _Atomic int in_callback;     // Timer is in callback (atomic)
    int pipe_read;              // Pipe read end
    int pipe_write;             // Pipe write end
    struct InfraxTimer* next;   // Next timer in wheel/heap
    struct InfraxTimer* prev;   // Previous timer in wheel/heap
    uint64_t expire_time;       // Expiration time in milliseconds
    _Atomic uint64_t expiry_count;      // Number of times timer has expired (atomic)
    _Atomic uint64_t last_notify_count; // Last notified expiry count (atomic)
};

// Time wheel for short timers
static struct {
    struct InfraxTimer* slots[WHEEL_SIZE];
    pthread_mutex_t mutex;  // 每个时间轮槽位一个锁
} wheel;

// Min heap for long timers
static struct {
    struct InfraxTimer** timers;  // Array of timer pointers
    size_t capacity;              // Current capacity
    size_t size;                  // Current size
    pthread_mutex_t mutex;        // 堆操作的锁
} heap;

// Timer thread state
typedef struct InfraxTimerThread {
    pthread_t thread;
    pthread_mutex_t mutex;
    _Atomic InfraxBool running;  // 原子运行状态
} InfraxTimerThread;

// Global timer thread instance
static InfraxTimerThread* g_timer_thread = NULL;

// Initialize wheel and heap
static void init_timer_system() {
    static int initialized = 0;
    if (!initialized) {
        // Initialize wheel
        memset(&wheel, 0, sizeof(wheel));
        pthread_mutex_init(&wheel.mutex, NULL);
        
        // Initialize heap
        memset(&heap, 0, sizeof(heap));
        pthread_mutex_init(&heap.mutex, NULL);
        heap.capacity = 16;
        heap.timers = calloc(heap.capacity, sizeof(struct InfraxTimer*));
        
        // Initialize timer thread
        InfraxError err = ensure_timer_thread();
        if (err.code != 0) {
            InfraxCoreClass.singleton()->printf(NULL, 
                "Failed to initialize timer thread: %s\n", err.message);
            return;
        }
        
        initialized = 1;
    }
}

// Create a new timer
static InfraxError infrax_timer_new(InfraxTimer** timer, int timeout_ms, 
                                   InfraxTimerCallback callback, void* arg) {
    InfraxError err = {0};
    
    // Initialize timer system if needed
    init_timer_system();
    
    // Allocate timer
    *timer = calloc(1, sizeof(struct InfraxTimer));
    if (!*timer) {
        err.code = INFRAX_ERROR_NO_MEMORY;
        snprintf(err.message, sizeof(err.message), "Failed to allocate timer");
        return err;
    }
    
    // Initialize timer
    (*timer)->timeout_ms = timeout_ms;
    (*timer)->callback = callback;
    (*timer)->callback_arg = arg;
    __atomic_store_n(&(*timer)->expiry_count, 0, __ATOMIC_SEQ_CST);
    __atomic_store_n(&(*timer)->last_notify_count, 0, __ATOMIC_SEQ_CST);
    
    // Create notification pipe
    int pipefd[2];
    if (pipe(pipefd) < 0) {
        err.code = INFRAX_ERROR_SYSTEM;
        snprintf(err.message, sizeof(err.message), "pipe() failed: %s", strerror(errno));
        close(pipefd[0]);
        close(pipefd[1]);
        free(*timer);
        *timer = NULL;
        return err;
    }
    
    // Make read end non-blocking
    int flags = fcntl(pipefd[0], F_GETFL, 0);
    if (flags < 0 || fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK) < 0) {
        err.code = INFRAX_ERROR_SYSTEM;
        snprintf(err.message, sizeof(err.message), "fcntl() failed: %s", strerror(errno));
        close(pipefd[0]);
        close(pipefd[1]);
        free(*timer);
        *timer = NULL;
        return err;
    }
    
    (*timer)->pipe_read = pipefd[0];
    (*timer)->pipe_write = pipefd[1];
    
    return err;
}

// Free a timer
static void infrax_timer_free(InfraxTimer* timer) {
    if (!timer) return;
    
    // Stop timer if active
    infrax_timer_stop(timer);
    
    // Close pipe
    close(timer->pipe_read);
    close(timer->pipe_write);
    
    // Free timer
    free(timer);
}

// Start the timer
static InfraxError infrax_timer_start(InfraxTimer* timer) {
    InfraxError err = {0};
    if (!timer) {
        err.code = INFRAX_ERROR_INVALID_PARAM;
        snprintf(err.message, sizeof(err.message), "Timer is NULL");
        return err;
    }
    
    // Set expiration time
    timer->expire_time = InfraxCoreClass.singleton()->time_monotonic_ms(
        InfraxCoreClass.singleton()) + timer->timeout_ms;
    
    // Add to appropriate container
    if (timer->timeout_ms <= WHEEL_MS) {
        // Add to wheel
        uint64_t slot = timer->expire_time & WHEEL_MASK;
        timer->next = wheel.slots[slot];
        if (wheel.slots[slot]) {
            wheel.slots[slot]->prev = timer;
        }
        wheel.slots[slot] = timer;
    } else {
        // Add to heap
        if (heap.size >= heap.capacity) {
            size_t new_capacity = heap.capacity * 2;
            struct InfraxTimer** new_timers = realloc(heap.timers, 
                new_capacity * sizeof(struct InfraxTimer*));
            if (!new_timers) {
                err.code = INFRAX_ERROR_NO_MEMORY;
                snprintf(err.message, sizeof(err.message), "Failed to grow heap");
                return err;
            }
            heap.timers = new_timers;
            heap.capacity = new_capacity;
        }
        
        // Add to heap and bubble up
        heap.timers[heap.size] = timer;
        size_t i = heap.size;
        while (i > 0) {
            size_t parent = (i - 1) / 2;
            if (heap.timers[parent]->expire_time <= timer->expire_time) {
                break;
            }
            heap.timers[i] = heap.timers[parent];
            i = parent;
        }
        heap.timers[i] = timer;
        heap.size++;
    }
    
    __atomic_store_n(&timer->active, 1, __ATOMIC_SEQ_CST);
    return err;
}

// Stop the timer
static void infrax_timer_stop(InfraxTimer* timer) {
    if (!timer || !__atomic_load_n(&timer->active, __ATOMIC_SEQ_CST)) return;
    
    // 等待回调完成
    while (__atomic_load_n(&timer->in_callback, __ATOMIC_SEQ_CST)) {
        usleep(1000);  // 等待1ms
    }
    
    // Remove from wheel/heap
    if (timer->timeout_ms <= WHEEL_MS) {
        pthread_mutex_lock(&wheel.mutex);
        if (timer->prev) {
            timer->prev->next = timer->next;
        } else {
            uint64_t slot = timer->expire_time & WHEEL_MASK;
            wheel.slots[slot] = timer->next;
        }
        if (timer->next) {
            timer->next->prev = timer->prev;
        }
        pthread_mutex_unlock(&wheel.mutex);
    } else {
        pthread_mutex_lock(&heap.mutex);
        // Find timer in heap
        for (size_t i = 0; i < heap.size; i++) {
            if (heap.timers[i] == timer) {
                // Remove from heap
                heap.size--;
                if (i < heap.size) {
                    // Move last timer to this position and bubble down
                    heap.timers[i] = heap.timers[heap.size];
                    while (1) {
                        size_t left = i * 2 + 1;
                        size_t right = left + 1;
                        if (left >= heap.size) break;
                        
                        size_t smallest = i;
                        if (heap.timers[left]->expire_time < heap.timers[i]->expire_time) {
                            smallest = left;
                        }
                        if (right < heap.size && 
                            heap.timers[right]->expire_time < heap.timers[smallest]->expire_time) {
                            smallest = right;
                        }
                        
                        if (smallest == i) break;
                        
                        struct InfraxTimer* tmp = heap.timers[i];
                        heap.timers[i] = heap.timers[smallest];
                        heap.timers[smallest] = tmp;
                        i = smallest;
                    }
                }
                break;
            }
        }
        pthread_mutex_unlock(&heap.mutex);
    }
    
    __atomic_store_n(&timer->active, 0, __ATOMIC_SEQ_CST);
}

// Reset the timer
static InfraxError infrax_timer_reset(InfraxTimer* timer, int timeout_ms) {
    InfraxError err = {0};
    if (!timer) {
        err.code = INFRAX_ERROR_INVALID_PARAM;
        snprintf(err.message, sizeof(err.message), "Timer is NULL");
        return err;
    }
    
    infrax_timer_stop(timer);
    timer->timeout_ms = timeout_ms;
    return infrax_timer_start(timer);
}

// Get timer file descriptor
static int infrax_timer_get_fd(const InfraxTimer* timer) {
    return timer ? timer->pipe_read : -1;
}

// Check and fire expired timers
void infrax_timer_check_expired() {
    uint64_t now = InfraxCoreClass.singleton()->time_monotonic_ms(
        InfraxCoreClass.singleton());
    
    // Check wheel
    pthread_mutex_lock(&wheel.mutex);
    uint64_t slot = now & WHEEL_MASK;
    struct InfraxTimer* timer = wheel.slots[slot];
    wheel.slots[slot] = NULL;
    pthread_mutex_unlock(&wheel.mutex);
    
    // Process all timers in this slot
    while (timer) {
        struct InfraxTimer* next = timer->next;
        if (timer->expire_time <= now) {
            // Timer expired
            if (__atomic_load_n(&timer->active, __ATOMIC_SEQ_CST)) {
                uint64_t count = __atomic_add_fetch(&timer->expiry_count, 1, __ATOMIC_SEQ_CST);
                uint64_t last_count = __atomic_load_n(&timer->last_notify_count, __ATOMIC_SEQ_CST);
                
                if (count > last_count) {
                    // 尝试写入 pipe，带错误处理
                    char byte = 1;  // 简化为单字节通知
                    ssize_t written = write(timer->pipe_write, &byte, sizeof(byte));
                    if (written == sizeof(byte)) {
                        __atomic_store_n(&timer->last_notify_count, count, __ATOMIC_SEQ_CST);
                        
                        // 调用回调函数
                        if (timer->callback) {
                            timer->callback(timer->callback_arg);
                        }
                    } else if (written < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            // 管道已满，稍后重试
                            __atomic_store_n(&timer->expiry_count, count - 1, __ATOMIC_SEQ_CST);
                        } else {
                            InfraxCoreClass.singleton()->printf(NULL, 
                                "Timer pipe write failed: %s\n", strerror(errno));
                        }
                    }
                }
            }
        } else {
            // Timer not expired, move to next slot
            pthread_mutex_lock(&wheel.mutex);
            uint64_t new_slot = timer->expire_time & WHEEL_MASK;
            timer->next = wheel.slots[new_slot];
            if (wheel.slots[new_slot]) {
                wheel.slots[new_slot]->prev = timer;
            }
            wheel.slots[new_slot] = timer;
            pthread_mutex_unlock(&wheel.mutex);
        }
        timer = next;
    }
    
    // Check heap
    pthread_mutex_lock(&heap.mutex);
    while (heap.size > 0) {
        if (heap.timers[0]->expire_time > now) {
            break;
        }
        
        // Get timer at root
        timer = heap.timers[0];
        
        // Remove from heap
        heap.size--;
        if (heap.size > 0) {
            heap.timers[0] = heap.timers[heap.size];
            // Bubble down
            size_t i = 0;
            while (1) {
                size_t left = i * 2 + 1;
                size_t right = left + 1;
                if (left >= heap.size) break;
                
                size_t smallest = i;
                if (heap.timers[left]->expire_time < heap.timers[i]->expire_time) {
                    smallest = left;
                }
                if (right < heap.size && 
                    heap.timers[right]->expire_time < heap.timers[smallest]->expire_time) {
                    smallest = right;
                }
                
                if (smallest == i) break;
                
                struct InfraxTimer* tmp = heap.timers[i];
                heap.timers[i] = heap.timers[smallest];
                heap.timers[smallest] = tmp;
                i = smallest;
            }
        }
        pthread_mutex_unlock(&heap.mutex);
        
        // Fire timer
        if (__atomic_load_n(&timer->active, __ATOMIC_SEQ_CST)) {
            uint64_t count = __atomic_add_fetch(&timer->expiry_count, 1, __ATOMIC_SEQ_CST);
            uint64_t last_count = __atomic_load_n(&timer->last_notify_count, __ATOMIC_SEQ_CST);
            
            if (count > last_count) {
                // 尝试写入 pipe，带错误处理
                char byte = 1;  // 简化为单字节通知
                ssize_t written = write(timer->pipe_write, &byte, sizeof(byte));
                if (written == sizeof(byte)) {
                    __atomic_store_n(&timer->last_notify_count, count, __ATOMIC_SEQ_CST);
                    
                    // 调用回调函数
                    if (timer->callback) {
                        timer->callback(timer->callback_arg);
                    }
                } else if (written < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        // 管道已满，稍后重试
                        __atomic_store_n(&timer->expiry_count, count - 1, __ATOMIC_SEQ_CST);
                    } else {
                        InfraxCoreClass.singleton()->printf(NULL, 
                            "Timer pipe write failed: %s\n", strerror(errno));
                    }
                }
            }
        }
        
        pthread_mutex_lock(&heap.mutex);
    }
    pthread_mutex_unlock(&heap.mutex);
}

// Get next expiration time
static uint64_t infrax_timer_next_expiration() {
    uint64_t next = UINT64_MAX;
    uint64_t now = InfraxCoreClass.singleton()->time_monotonic_ms(
        InfraxCoreClass.singleton());
    
    // Check wheel
    for (int i = 0; i < WHEEL_SIZE; i++) {
        struct InfraxTimer* timer = wheel.slots[i];
        while (timer) {
            if (timer->active && timer->expire_time < next) {
                next = timer->expire_time;
            }
            timer = timer->next;
        }
    }
    
    // Check heap
    if (heap.size > 0 && heap.timers[0]->active) {
        if (heap.timers[0]->expire_time < next) {
            next = heap.timers[0]->expire_time;
        }
    }
    
    return next;
}

// Timer thread function
static void* timer_thread_func(void* arg) {
    InfraxTimerThread* tt = (InfraxTimerThread*)arg;
    
    while (__atomic_load_n(&tt->running, __ATOMIC_SEQ_CST)) {
        // 检查过期的定时器
        infrax_timer_check_expired();
        
        // 计算下一个定时器的到期时间
        uint64_t next_expiry = infrax_timer_next_expiration();
        uint64_t now = InfraxCoreClass.singleton()->time_monotonic_ms(InfraxCoreClass.singleton());
        
        // 计算需要等待的时间
        uint64_t wait_time = (next_expiry == UINT64_MAX) ? 1000 : 
                            (next_expiry <= now) ? 1 : 
                            (next_expiry - now);
                            
        // 限制最大等待时间为1秒
        if (wait_time > 1000) wait_time = 1000;
        
        usleep(wait_time * 1000);  // 转换为微秒
    }
    
    return NULL;
}

// Initialize timer thread if needed
static InfraxError ensure_timer_thread(void) {
    InfraxError err = {0};
    
    if (g_timer_thread) {
        return err;  // Already initialized
    }
    
    // Allocate thread state
    g_timer_thread = (InfraxTimerThread*)malloc(sizeof(InfraxTimerThread));
    if (!g_timer_thread) {
        err.code = INFRAX_ERROR_NO_MEMORY;
        snprintf(err.message, sizeof(err.message), "Failed to allocate timer thread state");
        return err;
    }
    
    // Initialize mutex
    if (pthread_mutex_init(&g_timer_thread->mutex, NULL) != 0) {
        err.code = INFRAX_ERROR_SYSTEM;
        snprintf(err.message, sizeof(err.message), "Failed to init mutex: %s", strerror(errno));
        free(g_timer_thread);
        g_timer_thread = NULL;
        return err;
    }
    
    __atomic_store_n(&g_timer_thread->running, 1, __ATOMIC_SEQ_CST);
    
    // Start thread
    if (pthread_create(&g_timer_thread->thread, NULL, timer_thread_func, g_timer_thread) != 0) {
        err.code = INFRAX_ERROR_SYSTEM;
        snprintf(err.message, sizeof(err.message), "Failed to create thread: %s", strerror(errno));
        pthread_mutex_destroy(&g_timer_thread->mutex);
        free(g_timer_thread);
        g_timer_thread = NULL;
        return err;
    }
    
    return err;
}

// Check if timer is in callback
static InfraxBool infrax_timer_is_in_callback(const InfraxTimer* timer) {
    if (!timer) return INFRAX_FALSE;
    return __atomic_load_n(&timer->in_callback, __ATOMIC_SEQ_CST) ? INFRAX_TRUE : INFRAX_FALSE;
}

// Wait for callback to complete
static void infrax_timer_wait_callback(const InfraxTimer* timer) {
    if (!timer) return;
    while (__atomic_load_n(&timer->in_callback, __ATOMIC_SEQ_CST)) {
        usleep(1000);  // 等待1ms
    }
}

// Global timer class instance
InfraxTimerClassType InfraxTimerClass = {
    .new = infrax_timer_new,
    .free = infrax_timer_free,
    .start = infrax_timer_start,
    .stop = infrax_timer_stop,
    .reset = infrax_timer_reset,
    .get_fd = infrax_timer_get_fd,
    .next_expiration = infrax_timer_next_expiration,
    .is_in_callback = infrax_timer_is_in_callback,
    .wait_callback = infrax_timer_wait_callback
};
