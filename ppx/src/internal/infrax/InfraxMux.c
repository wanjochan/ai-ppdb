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
    int pipe_read;
    int pipe_write;
    InfraxBool running;
    pthread_mutex_t mutex;
    InfraxTimer* timer;
    InfraxU32 next_timer_id;  // For generating unique timer IDs
} InfraxMuxTimerThread;

// Global timer thread instance
static InfraxMuxTimerThread* g_timer_thread = NULL;

// Timer thread function
static void* timer_thread_func(void* arg) {
    InfraxMuxTimerThread* tt = (InfraxMuxTimerThread*)arg;
    
    while (tt->running) {
        // Check timer expiration
        pthread_mutex_lock(&tt->mutex);
        InfraxTime next = InfraxTimerClass.next_expiration(tt->timer);
        if (next >= 0) {
            InfraxTime now = InfraxCoreClass.singleton()->time_monotonic_ms(InfraxCoreClass.singleton());
            if (now >= next) {
                // Timer expired, notify through pipe
                char c = '!';
                write(tt->pipe_write, &c, 1);
            }
        }
        pthread_mutex_unlock(&tt->mutex);
        
        // Sleep a bit to avoid busy loop
        usleep(1000);  // 1ms
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
    g_timer_thread = (InfraxMuxTimerThread*)malloc(sizeof(InfraxMuxTimerThread));
    if (!g_timer_thread) {
        err.code = INFRAX_ERROR_NO_MEMORY;
        snprintf(err.message, sizeof(err.message), "Failed to allocate timer thread state");
        return err;
    }
    
    // Create pipe
    int pipefd[2];
    if (pipe(pipefd) != 0) {
        err.code = INFRAX_ERROR_SYSTEM;
        snprintf(err.message, sizeof(err.message), "Failed to create pipe: %s", strerror(errno));
        free(g_timer_thread);
        g_timer_thread = NULL;
        return err;
    }
    
    g_timer_thread->pipe_read = pipefd[0];
    g_timer_thread->pipe_write = pipefd[1];
    g_timer_thread->next_timer_id = 1;  // Start from 1
    
    // Initialize mutex
    if (pthread_mutex_init(&g_timer_thread->mutex, NULL) != 0) {
        err.code = INFRAX_ERROR_SYSTEM;
        snprintf(err.message, sizeof(err.message), "Failed to init mutex: %s", strerror(errno));
        close(g_timer_thread->pipe_read);
        close(g_timer_thread->pipe_write);
        free(g_timer_thread);
        g_timer_thread = NULL;
        return err;
    }
    
    // Create timer
    err = InfraxTimerClass.new(&g_timer_thread->timer, 0, NULL, NULL);
    if (err.code != 0) {
        pthread_mutex_destroy(&g_timer_thread->mutex);
        close(g_timer_thread->pipe_read);
        close(g_timer_thread->pipe_write);
        free(g_timer_thread);
        g_timer_thread = NULL;
        return err;
    }
    
    // Start thread
    g_timer_thread->running = 1;
    if (pthread_create(&g_timer_thread->thread, NULL, timer_thread_func, g_timer_thread) != 0) {
        err.code = INFRAX_ERROR_SYSTEM;
        snprintf(err.message, sizeof(err.message), "Failed to create thread: %s", strerror(errno));
        InfraxTimerClass.free(g_timer_thread->timer);
        pthread_mutex_destroy(&g_timer_thread->mutex);
        close(g_timer_thread->pipe_read);
        close(g_timer_thread->pipe_write);
        free(g_timer_thread);
        g_timer_thread = NULL;
        return err;
    }
    
    return err;
}

// Set timeout
static InfraxU32 infrax_mux_set_timeout(InfraxU32 interval_ms, InfraxMuxHandler handler, void* arg) {
    // Ensure timer thread is running
    InfraxError err = ensure_timer_thread();
    if (err.code != 0) {
        return 0;  // Return 0 as invalid timer ID
    }
    
    pthread_mutex_lock(&g_timer_thread->mutex);
    
    // Get next timer ID
    InfraxU32 timer_id = g_timer_thread->next_timer_id++;
    
    // Start timer
    err = InfraxTimerClass.start(g_timer_thread->timer);
    if (err.code != 0) {
        pthread_mutex_unlock(&g_timer_thread->mutex);
        return 0;  // Return 0 as invalid timer ID
    }
    
    pthread_mutex_unlock(&g_timer_thread->mutex);
    return timer_id;
}

// Clear timeout
static InfraxError infrax_mux_clear_timeout(InfraxU32 timer_id) {
    InfraxError err = {0};
    
    if (!g_timer_thread) {
        return err;  // No timer running
    }
    
    pthread_mutex_lock(&g_timer_thread->mutex);
    InfraxTimerClass.stop(g_timer_thread->timer);  // stop() returns void
    pthread_mutex_unlock(&g_timer_thread->mutex);
    
    return err;
}

// Poll for events
static InfraxError infrax_mux_pollall(const int* fds, size_t nfds, InfraxMuxHandler handler, void* arg, int timeout_ms) {
    InfraxError err = {0};
    struct pollfd* pfds = NULL;
    
    // Add pipe fd if timer thread exists
    size_t total_fds = nfds;
    if (g_timer_thread) {
        total_fds++;
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
    
    // Add pipe fd
    if (g_timer_thread) {
        pfds[nfds].fd = g_timer_thread->pipe_read;
        pfds[nfds].events = POLLIN;
        pfds[nfds].revents = 0;
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
    
    // Call handler for ready descriptors
    if (handler) {
        for (size_t i = 0; i < total_fds; i++) {
            if (pfds[i].revents) {
                if (g_timer_thread && pfds[i].fd == g_timer_thread->pipe_read) {
                    // Handle timer event
                    char buf[1];
                    read(pfds[i].fd, buf, 1);  // Clear pipe
                    handler(pfds[i].fd, POLLIN, arg);
                } else {
                    handler(pfds[i].fd, pfds[i].revents, arg);
                }
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
