#include "internal/infrax/InfraxMux.h"
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>

// Timer thread state
typedef struct InfraxMuxTimerThread {
    pthread_t thread;
    pthread_mutex_t mutex;
    InfraxBool running;
    InfraxTimer* timer;  // 使用 InfraxTimer
    InfraxMuxTimer* timers;  // 定时器链表
    InfraxU32 next_timer_id;  // For generating unique timer IDs
} InfraxMuxTimerThread;

// Global timer thread instance
static InfraxMuxTimerThread* g_timer_thread = NULL;

// Timer thread function
static void* timer_thread_func(void* arg) {
    InfraxMuxTimerThread* tt = (InfraxMuxTimerThread*)arg;
    
    while (tt->running) {
        pthread_mutex_lock(&tt->mutex);
        
        // Check timers
        InfraxMuxTimer* timer = tt->timers;
        while (timer) {
            if (timer->active) {
                infrax_timer_check_expired();
            }
            timer = timer->next;
        }
        
        pthread_mutex_unlock(&tt->mutex);
        usleep(1000);  // 1ms
    }
    
    return NULL;
}

// Timer callback
static void timer_callback(void* arg) {
    InfraxMuxTimer* timer = (InfraxMuxTimer*)arg;
    if (timer && timer->active && timer->handler) {
        timer->handler(InfraxTimerClass.get_fd(timer->infrax_timer), POLLIN, timer->arg);
    }
}

// Initialize timer thread if needed
static InfraxError ensure_timer_thread(void) {
    InfraxError err = {0};
    
    if (g_timer_thread) {
        return err;  // Already initialized
    }
    
    // Allocate thread state
    g_timer_thread = (InfraxMuxTimerThread*)malloc(sizeof(InfraxMuxTimerThread));
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
    
    g_timer_thread->next_timer_id = 1;  // Start from 1
    g_timer_thread->timers = NULL;  // 初始化定时器链表为空
    g_timer_thread->timer = NULL;  // 不再需要全局定时器
    
    // Start thread
    g_timer_thread->running = 1;
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

// Set timeout
static InfraxU32 infrax_mux_set_timeout(InfraxU32 interval_ms, InfraxMuxHandler handler, void* arg) {
    return InfraxTimerClass.create_mux_timer(interval_ms, handler, arg);
}

// Clear timeout
static InfraxError infrax_mux_clear_timeout(InfraxU32 timer_id) {
    return InfraxTimerClass.clear_mux_timer(timer_id);
}

// Poll for events
static InfraxError infrax_mux_pollall(const int* fds, size_t nfds, InfraxMuxHandler handler, void* arg, int timeout_ms) {
    InfraxError err = {0};
    struct pollfd* pfds = NULL;
    
    // Add timer fds
    size_t total_fds = nfds;
    InfraxMuxTimer* timers = InfraxTimerClass.get_active_mux_timers();
    InfraxMuxTimer* timer = timers;
    while (timer) {
        if (timer->active) {
            total_fds++;
        }
        timer = timer->next;
    }
    
    // Allocate pollfd array
    pfds = (struct pollfd*)malloc(total_fds * sizeof(struct pollfd));
    if (!pfds) {
        err.code = INFRAX_ERROR_NO_MEMORY;
        snprintf(err.message, sizeof(err.message), "Failed to allocate memory for pollfd array");
        return err;
    }
    
    // Initialize pollfd structures
    for (size_t i = 0; i < nfds; i++) {
        pfds[i].fd = fds[i];
        pfds[i].events = POLLIN;  // Default to watching for read events
        pfds[i].revents = 0;
    }
    
    // Add timer fds
    size_t idx = nfds;
    timer = timers;
    while (timer) {
        if (timer->active) {
            pfds[idx].fd = InfraxTimerClass.get_fd(timer->infrax_timer);
            pfds[idx].events = POLLIN;
            pfds[idx].revents = 0;
            idx++;
        }
        timer = timer->next;
    }
    
    // Do the poll
    int ret = poll(pfds, total_fds, timeout_ms);
    if (ret < 0) {
        err.code = INFRAX_ERROR_SYSTEM;
        snprintf(err.message, sizeof(err.message), "poll() failed: %s", strerror(errno));
        free(pfds);
        return err;
    }
    
    // Handle timeout
    if (ret == 0) {
        err.code = INFRAX_ERROR_TIMEOUT;
        snprintf(err.message, sizeof(err.message), "poll() timed out after %d ms", timeout_ms);
        free(pfds);
        return err;
    }
    
    // Process events
    for (size_t i = 0; i < total_fds; i++) {
        if (pfds[i].revents) {
            if (i >= nfds) {
                // Handle timer event
                timer = timers;
                while (timer) {
                    if (timer->active && pfds[i].fd == InfraxTimerClass.get_fd(timer->infrax_timer)) {
                        infrax_timer_check_expired();
                        if (timer->handler) {
                            timer->handler(pfds[i].fd, pfds[i].revents, timer->arg);
                        }
                        break;
                    }
                    timer = timer->next;
                }
            } else if (handler) {
                handler(pfds[i].fd, pfds[i].revents, arg);
            }
        }
    }
    
    free(pfds);
    return err;
}

// Global class instance
InfraxMuxClassType InfraxMuxClass = {
    .setTimeout = infrax_mux_set_timeout,
    .clearTimeout = infrax_mux_clear_timeout,
    .pollall = infrax_mux_pollall
};
